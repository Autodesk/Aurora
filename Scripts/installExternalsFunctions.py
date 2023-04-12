#
# Copyright 2017 Pixar
# Copyright 2023 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#

from __future__ import print_function

import argparse
import contextlib
import datetime
import fnmatch
import glob
import locale
import multiprocessing
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import sysconfig
import tarfile
import zipfile
import pathlib
from shutil import which
from urllib.request import urlopen

# Helpers for printing output
verbosity = 1

# Script version, incrementing this will force re-installation of all packages.
scriptVersion = "00001"

def Print(msg):
    if verbosity > 0:
        print(msg)

def PrintWarning(warning):
    if verbosity > 0:
        print("WARNING:", warning)

def PrintStatus(status):
    if verbosity >= 1:
        print("STATUS:", status)

def PrintInfo(info):
    if verbosity >= 2:
        print("INFO:", info)

def PrintCommandOutput(output):
    if verbosity >= 3:
        sys.stdout.write(output)

def PrintError(error):
    if verbosity >= 3 and sys.exc_info()[1] is not None:
        import traceback
        traceback.print_exc()
    print ("ERROR:", error)

# Helpers for determining platform
def Windows():
    return platform.system() == "Windows"
def Linux():
    return platform.system() == "Linux"

def GetLocale():
    return sys.stdout.encoding or locale.getdefaultlocale()[1] or "UTF-8"

def GetCommandOutput(command):
    """Executes the specified command and returns output or None."""
    try:
        return subprocess.check_output(shlex.split(command),
            stderr=subprocess.STDOUT).decode(GetLocale(), 'replace').strip()
    except subprocess.CalledProcessError:
        pass
    return None

def GetVisualStudioCompilerAndVersion():
    """
    Returns a tuple containing the path to the Visual Studio compiler
    and a tuple for its version, e.g. (14, 0). If the compiler is not found
    or version number cannot be determined, returns None.
    """
    if not Windows():
        return None

    msvcCompiler = which('cl')
    if msvcCompiler:
        # VisualStudioVersion environment variable should be set by the
        # Visual Studio Command Prompt.
        match = re.search(r"(\d+)\.(\d+)",
                          os.environ.get("VisualStudioVersion", ""))
        if match:
            return (msvcCompiler, tuple(int(v) for v in match.groups()))
    return None

def IsVisualStudioVersionOrGreater(desiredVersion):
    if not Windows():
        return False

    msvcCompilerAndVersion = GetVisualStudioCompilerAndVersion()
    if msvcCompilerAndVersion:
        _, version = msvcCompilerAndVersion
        return version >= desiredVersion
    return False

def IsVisualStudio2019OrGreater():
    VISUAL_STUDIO_2019_VERSION = (16, 0)
    return IsVisualStudioVersionOrGreater(VISUAL_STUDIO_2019_VERSION)

def GetPythonInfo(context):
    """Returns a tuple containing the path to the Python executable, shared
    library, and include directory corresponding to the version of Python
    currently running. Returns None if any path could not be determined.

    This function is used to extract build information from the Python
    interpreter used to launch this script. This information is used
    in the Boost and USD builds. By taking this approach we can support
    having USD builds for different Python versions built on the same
    machine. This is very useful, especially when developers have multiple
    versions installed on their machine, which is quite common now with
    Python2 and Python3 co-existing.
    """

    # If we were given build python info then just use it.
    if context.build_python_info:
        return (context.build_python_info['PYTHON_EXECUTABLE'],
                context.build_python_info['PYTHON_LIBRARY'],
                context.build_python_info['PYTHON_INCLUDE_DIR'],
                context.build_python_info['PYTHON_VERSION'])

    # First we extract the information that can be uniformly dealt with across
    # the platforms:
    pythonExecPath = sys.executable
    pythonVersion = sysconfig.get_config_var("py_version_short")  # "2.7"

    # Lib path is unfortunately special for each platform and there is no
    # config_var for it. But we can deduce it for each platform, and this
    # logic works for any Python version.
    def _GetPythonLibraryFilename(context):
        if Windows():
            return "python{version}.lib".format(
                version=sysconfig.get_config_var("py_version_nodot"))
        elif Linux():
            return sysconfig.get_config_var("LDLIBRARY")
        else:
            raise RuntimeError("Platform not supported")

    pythonIncludeDir = sysconfig.get_path("include")
    if not pythonIncludeDir or not os.path.isdir(pythonIncludeDir):
        # as a backup, and for legacy reasons - not preferred because
        # it may be baked at build time
        pythonIncludeDir = sysconfig.get_config_var("INCLUDEPY")

    # if in a venv, installed_base will be the "original" python,
    # which is where the libs are ("base" will be the venv dir)
    pythonBaseDir = sysconfig.get_config_var("installed_base")
    if not pythonBaseDir or not os.path.isdir(pythonBaseDir):
        # for python-2.7
        pythonBaseDir = sysconfig.get_config_var("base")

    if Windows():
        pythonLibPath = os.path.join(pythonBaseDir, "libs",
                                     _GetPythonLibraryFilename(context))
    elif Linux():
        pythonMultiarchSubdir = sysconfig.get_config_var("multiarchsubdir")
        # Try multiple ways to get the python lib dir
        for pythonLibDir in (sysconfig.get_config_var("LIBDIR"),
                             os.path.join(pythonBaseDir, "lib")):
            if pythonMultiarchSubdir:
                pythonLibPath = \
                    os.path.join(pythonLibDir + pythonMultiarchSubdir,
                                 _GetPythonLibraryFilename(context))
                if os.path.isfile(pythonLibPath):
                    break
            pythonLibPath = os.path.join(pythonLibDir,
                                         _GetPythonLibraryFilename(context))
            if os.path.isfile(pythonLibPath):
                break
    else:
        raise RuntimeError("Platform not supported")

    return (pythonExecPath, pythonLibPath, pythonIncludeDir, pythonVersion)

# Check the installed version of a package, using generated .version.txt file.
def CheckVersion(context, packageName, versionString):
    fullVersionString = scriptVersion+":"+versionString
    versionTextFilename = os.path.join(context.externalsInstDir, packageName+".version.txt")
    if(not os.path.exists(versionTextFilename)):
        return False

    versionTxt = pathlib.Path(versionTextFilename).read_text()
    return versionTxt==fullVersionString
    
# Update generated .version.txt file for a package.
def UpdateVersion(context, packageName, versionString):
    if(CheckVersion(context, packageName, versionString)):
        return
    fullVersionString = scriptVersion+":"+versionString
    versionTextFilename = os.path.join(context.externalsInstDir, packageName+".version.txt")
    versionFile= open(versionTextFilename, "wt")
    versionFile.write(fullVersionString)
    versionFile.close()

def GetCPUCount():
    try:
        return multiprocessing.cpu_count()
    except NotImplementedError:
        return 1

def Run(cmd, logCommandOutput = True):
    """
    Run the specified command in a subprocess.
    """
    PrintInfo('Running "{cmd}"'.format(cmd=cmd))

    with open("log.txt", mode="a", encoding="utf-8") as logfile:
        logfile.write(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))
        logfile.write("\n")
        logfile.write(cmd)
        logfile.write("\n")

        # Let exceptions escape from subprocess calls -- higher level
        # code will handle them.
        if logCommandOutput:
            p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
            while True:
                l = p.stdout.readline().decode(GetLocale(), 'replace')
                if l:
                    logfile.write(l)
                    PrintCommandOutput(l)
                elif p.poll() is not None:
                    break
        else:
            p = subprocess.Popen(shlex.split(cmd))
            p.wait()

    if p.returncode != 0:
        # If verbosity >= 3, we'll have already been printing out command output
        # so no reason to print the log file again.
        if verbosity < 3:
            with open("log.txt", "r") as logfile:
                Print(logfile.read())
        raise RuntimeError("Failed to run '{cmd}'\nSee {log} for more details."
                           .format(cmd=cmd, log=os.path.abspath("log.txt")))

@contextlib.contextmanager
def CurrentWorkingDirectory(dir):
    """
    Context manager that sets the current working directory to the given
    directory and resets it to the original directory when closed.
    """
    curdir = os.getcwd()
    os.chdir(dir)
    try: yield
    finally: os.chdir(curdir)

def MakeSymLink(context, src):
    """
    Create a symbolic file of dest to src
    """
    patternToLinkTo = src
    filesToLinkTo = glob.glob(patternToLinkTo)
    for linkTo in filesToLinkTo:
        symlink = pathlib.Path(linkTo).with_suffix('')
        if symlink.exists():
            os.remove(symlink)
        PrintCommandOutput(f"Create symlink {symlink} to {linkTo}\n")
        os.symlink(linkTo, symlink)

def CopyFiles(context, src, dest, destPrefix = ''):
    """
    Copy files like shutil.copy, but src may be a glob pattern.
    """
    filesToCopy = glob.glob(src)
    if not filesToCopy:
        raise RuntimeError("File(s) to copy {src} not found".format(src=src))

    instDestDir = os.path.join(context.externalsInstDir, destPrefix, dest)
    if not os.path.isdir(instDestDir):
        os.makedirs(instDestDir)

    for f in filesToCopy:
        PrintCommandOutput("Copying {file} to {destDir}\n"
                           .format(file=f, destDir=instDestDir))
        shutil.copy(f, instDestDir)

def CopyDirectory(context, srcDir, destDir, destPrefix = ''):
    """
    Copy directory like shutil.copytree.
    """
    instDestDir = os.path.join(context.externalsInstDir, destPrefix, destDir)
    if os.path.isdir(instDestDir):
        shutil.rmtree(instDestDir)

    PrintCommandOutput("Copying {srcDir} to {destDir}\n"
                       .format(srcDir=srcDir, destDir=instDestDir))
    shutil.copytree(srcDir, instDestDir)

def FormatMultiProcs(numJobs, generator):
    tag = "-j"
    if generator:
        if "Visual Studio" in generator:
            tag = "/M:" # This will build multiple projects at once.

    return "{tag}{procs}".format(tag=tag, procs=numJobs)

def BuildConfigs(context):
    configs = []
    if context.buildDebug:
        configs.append("Debug")
    if context.buildRelease:
        configs.append("Release")
    if context.buildRelWithDebInfo :
        configs.append("RelWithDebInfo")
    return configs

def RunCMake(context, force, instFolder= None, extraArgs = None,  configExtraArgs = None, install = True):
    """
    Invoke CMake to configure, build, and install a library whose
    source code is located in the current working directory.
    """
    # Create a directory for out-of-source builds in the build directory
    # using the name of the current working directory.
    srcDir = os.getcwd()
    generator = context.cmakeGenerator

    if generator is not None:
        generator = '-G "{gen}"'.format(gen=generator)
    elif IsVisualStudio2019OrGreater():
        generator = '-G "Visual Studio 16 2019" -A x64'

    toolset = context.cmakeToolset
    if toolset is not None:
        toolset = '-T "{toolset}"'.format(toolset=toolset)

    for config in BuildConfigs(context):
        buildDir = os.path.join(context.buildDir, os.path.split(srcDir)[1], config)
        if force and os.path.isdir(buildDir):
            shutil.rmtree(buildDir)
        if not os.path.isdir(buildDir):
            os.makedirs(buildDir)

        subFolder = instFolder if instFolder else os.path.basename(srcDir)
        instDir = os.path.join(context.externalsInstDir, subFolder)

        context.cmakePrefixPaths.add(instDir)

        with CurrentWorkingDirectory(buildDir):
            # We use -DCMAKE_BUILD_TYPE for single-configuration generators
            # (Ninja, make), and --config for multi-configuration generators
            # (Visual Studio); technically we don't need BOTH at the same
            # time, but specifying both is simpler than branching
            Run('cmake '
                '-DCMAKE_INSTALL_PREFIX="{instDir}" '
                '-DCMAKE_PREFIX_PATH="{prefixPaths}" '
                '-DCMAKE_BUILD_TYPE={config} '
                '-DCMAKE_DEBUG_POSTFIX="d" '
                '{generator} '
                '{toolset} '
                '{extraArgs} '
                '{configExtraArgs} '
                '"{srcDir}"'
                .format(instDir=instDir,
                        prefixPaths=';'.join(context.cmakePrefixPaths),
                        config=config,
                        srcDir=srcDir,
                        generator=(generator or ""),
                        toolset=(toolset or ""),
                        extraArgs=(" ".join(extraArgs) if extraArgs else ""),
                        configExtraArgs=(configExtraArgs[config] if configExtraArgs else "")))

            Run("cmake --build . --config {config} {install} -- {multiproc}"
                .format(config=config,
                        install=("--target install" if install else ""),
                        multiproc=FormatMultiProcs(context.numJobs, generator)))

def GetCMakeVersion():
    """
    Returns the CMake version as tuple of integers (major, minor) or
    (major, minor, patch) or None if an error occurred while launching cmake and
    parsing its output.
    """
    output_string = GetCommandOutput("cmake --version")
    if not output_string:
        PrintWarning("Could not determine cmake version -- please install it "
                     "and adjust your PATH")
        return None

    # cmake reports, e.g., "... version 3.14.3"
    match = re.search(r"version (\d+)\.(\d+)(\.(\d+))?", output_string)
    if not match:
        PrintWarning("Could not determine cmake version")
        return None

    major, minor, patch_group, patch = match.groups()
    if patch_group is None:
        return (int(major), int(minor))
    else:
        return (int(major), int(minor), int(patch))

def PatchFile(filename, patches, multiLineMatches=False):
    """
    Applies patches to the specified file. patches is a list of tuples
    (old string, new string).
    """
    if multiLineMatches:
        oldLines = [open(filename, 'r').read()]
    else:
        oldLines = open(filename, 'r').readlines()
    newLines = oldLines
    for (oldString, newString) in patches:
        newLines = [s.replace(oldString, newString) for s in newLines]
    if newLines != oldLines:
        PrintInfo("Patching file {filename} (original in {oldFilename})..."
                  .format(filename=filename, oldFilename=filename + ".old"))
        shutil.copy(filename, filename + ".old")
        open(filename, 'w').writelines(newLines)

def ApplyGitPatch(patchfile):
    try:
        patch = os.path.normpath(os.path.join(context.auroraSrcDir, "Scripts", "Patches", patchfile))
        PrintStatus(f"Applying {patchfile} ...")
        Run(f'git apply "{patch}"')
    except Exception as e:
        PrintWarning(f"Failed to apply {patchfile}. Skipped\n")



def DownloadFileWithUrllib(url, outputFilename):
    r = urlopen(url)
    with open(outputFilename, "wb") as outfile:
        outfile.write(r.read())

def DownloadURL(url, context, force, extractDir = None, dontExtract = None, destDir = None):
    """
    Download and extract the archive file at given URL to the
    source directory specified in the context.

    dontExtract may be a sequence of path prefixes that will
    be excluded when extracting the archive.

    Returns the absolute path to the directory where files have
    been extracted.
    """
    with CurrentWorkingDirectory(context.externalsSrcDir):
        # Extract filename from URL and see if file already exists.
        filename = url.split("/")[-1]
        if force and os.path.exists(filename):
            os.remove(filename)

        if os.path.exists(filename):
            PrintInfo("{0} already exists, skipping download"
                      .format(os.path.abspath(filename)))
        else:
            PrintInfo("Downloading {0} to {1}"
                      .format(url, os.path.abspath(filename)))

            # To work around occasional hiccups with downloading from websites
            # (SSL validation errors, etc.), retry a few times if we don't
            # succeed in downloading the file.
            maxRetries = 5
            lastError = None

            # Download to a temporary file and rename it to the expected
            # filename when complete. This ensures that incomplete downloads
            # will be retried if the script is run again.
            tmpFilename = filename + ".tmp"
            if os.path.exists(tmpFilename):
                os.remove(tmpFilename)

            for i in range(maxRetries):
                try:
                    context.downloader(url, tmpFilename)
                    break
                except Exception as e:
                    PrintCommandOutput("Retrying download due to error: {err}\n"
                                       .format(err=e))
                    lastError = e
            else:
                errorMsg = str(lastError)
                raise RuntimeError("Failed to download {url}: {err}"
                                   .format(url=url, err=errorMsg))

            shutil.move(tmpFilename, filename)

        # Open the archive and retrieve the name of the top-most directory.
        # This assumes the archive contains a single directory with all
        # of the contents beneath it, unless a specific extractDir is specified,
        # which is to be used.
        archive = None
        rootDir = None
        members = None
        try:
            if tarfile.is_tarfile(filename):
                archive = tarfile.open(filename)
                if extractDir:
                    rootDir = extractDir
                else:
                    rootDir = archive.getnames()[0].split('/')[0]
                if dontExtract != None:
                    members = (m for m in archive.getmembers()
                               if not any((fnmatch.fnmatch(m.name, p)
                                           for p in dontExtract)))
            elif zipfile.is_zipfile(filename):
                archive = zipfile.ZipFile(filename)
                if extractDir:
                    rootDir = extractDir
                else:
                    rootDir = archive.namelist()[0].split('/')[0]
                if dontExtract != None:
                    members = (m for m in archive.getnames()
                               if not any((fnmatch.fnmatch(m, p)
                                           for p in dontExtract)))
            else:
                raise RuntimeError("unrecognized archive file type")

            with archive:
                extractedPath = os.path.abspath(destDir if destDir else rootDir)

                if force and os.path.isdir(extractedPath):
                    shutil.rmtree(extractedPath)

                if os.path.isdir(extractedPath):
                    PrintInfo("Directory {0} already exists, skipping extract"
                              .format(extractedPath))
                else:
                    PrintInfo("Extracting archive to {0}".format(extractedPath))

                    # Extract to a temporary directory then move the contents
                    # to the expected location when complete. This ensures that
                    # incomplete extracts will be retried if the script is run
                    # again.
                    tmpExtractedPath = os.path.abspath("extract_dir")
                    if os.path.isdir(tmpExtractedPath):
                        shutil.rmtree(tmpExtractedPath)

                    if destDir:
                        archive.extractall(os.path.join(tmpExtractedPath, destDir), members=members)
                        shutil.move(os.path.join(tmpExtractedPath, destDir), extractedPath)
                    else:
                        archive.extractall(tmpExtractedPath, members=members)
                        shutil.move(os.path.join(tmpExtractedPath, rootDir), extractedPath)

                    if os.path.isdir(tmpExtractedPath):
                        shutil.rmtree(tmpExtractedPath)

                return extractedPath
        except Exception as e:
            # If extraction failed for whatever reason, assume the
            # archive file was bad and move it aside so that re-running
            # the script will try downloading and extracting again.
            shutil.move(filename, filename + ".bad")
            raise RuntimeError("Failed to extract archive {filename}: {err}"
                               .format(filename=filename, err=e))

def IsGitFolder(path = '.'):
    return subprocess.call(['git', '-C', path, 'status'],
                           stderr=subprocess.STDOUT,
                           stdout = open(os.devnull, 'w')) == 0

def GitClone(url, tag, cloneDir, context):
    try:
        with CurrentWorkingDirectory(context.externalsSrcDir):
            # TODO check if cloneDir is a cloned folder of url
            if not os.path.exists(cloneDir):
                Run("git clone --recurse-submodules -b {tag} {url} {folder}".format(
                    tag=tag, url=url, folder=cloneDir))
            elif not IsGitFolder(cloneDir):
                raise RuntimeError("Failed to clone repo {url} ({tag}): non-git folder {folder} exists".format(
                                    url=url, tag=tag, folder=cloneDir))
            return os.path.abspath(cloneDir)
    except Exception as e:
        raise RuntimeError("Failed to clone repo {url} ({tag}): {err}".format(
                            url=url, tag=tag, err=e))

def GitCloneSHA(url, sha, cloneDir, context):
    try:
        with CurrentWorkingDirectory(context.externalsSrcDir):
            # TODO check if cloneDir is a cloned folder of url
            if not os.path.exists(cloneDir):
                Run("git clone --recurse-submodules {url} {folder}".format(url=url, folder=cloneDir))
                with CurrentWorkingDirectory(os.path.join(context.externalsSrcDir, cloneDir)):
                    Run("git checkout {sha}".format(sha=sha))
            elif not IsGitFolder(cloneDir):
                raise RuntimeError("Failed to clone repo {url} ({tag}): non-git folder {folder} exists".format(
                                    url=url, tag=tag, folder=cloneDir))
            return os.path.abspath(cloneDir)
    except Exception as e:
        raise RuntimeError("Failed to clone repo {url} ({tag}): {err}".format(
                            url=url, tag=tag, err=e))

def WriteExternalsConfig(context, externals):
    win32Header = """
# Build configurations: {buildConfiguration}
if(WIN32)
  message(STATUS "Supported build configurations: {buildConfiguration}")
endif()
""".format(buildConfiguration=";".join(context.buildConfigs))

    header = """
# Auto-generated by installExternals.py. Any modification will be overridden
# by the next run of installExternals.py.
{win32Header}

if(NOT DEFINED EXTERNALS_ROOT)
    set(EXTERNALS_ROOT "{externalsRoot}")
endif()

set(AURORA_DEPENDENCIES "")
""".format(externalsRoot=pathlib.Path(context.externalsInstDir).as_posix(),
           win32Header=win32Header if Windows() else "")

    package = """
if(NOT DEFINED {packageName}_ROOT)
    set({packageName}_ROOT "${{EXTERNALS_ROOT}}/{installFolder}")
endif()
list(APPEND AURORA_DEPENDENCIES "${{{packageName}_ROOT}}")
# find_package_verbose({packageName})
"""
    packages = ""
    for ext in externals:
        packages += package.format(packageName=ext.packageName, installFolder=ext.installFolder)

    externalConfig = os.path.join(context.auroraSrcDir, "Scripts", "cmake", "externalsConfig.cmake")
    print("Written CMake config file: "+externalConfig)
    with open(externalConfig, "w") as f:
        f.write(header)
        f.write(packages)
