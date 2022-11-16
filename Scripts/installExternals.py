#
# Copyright 2017 Pixar
# Copyright 2022 Autodesk
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
import codecs
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

from urllib.request import urlopen
from shutil import which

if sys.version_info.major < 3:
    raise Exception("Python 3 or a more recent version is required.")

# Helpers for printing output
verbosity = 1

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

    with codecs.open("log.txt", "a", "utf-8") as logfile:
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

def CopyFiles(context, src, dest, destPrefix):
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

def CopyDirectory(context, srcDir, destDir, destPrefix):
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

def RunCMake(context, force, extraArgs = None,  configExtraArgs = None, install = True):
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

        instDir = os.path.join(context.externalsInstDir, config)

        with CurrentWorkingDirectory(buildDir):
            # We use -DCMAKE_BUILD_TYPE for single-configuration generators
            # (Ninja, make), and --config for multi-configuration generators
            # (Visual Studio); technically we don't need BOTH at the same
            # time, but specifying both is simpler than branching
            Run('cmake '
                '-DCMAKE_INSTALL_PREFIX="{instDir}" '
                '-DCMAKE_PREFIX_PATH="{instDir}" '
                '-DCMAKE_BUILD_TYPE={config} '
                '{generator} '
                '{toolset} '
                '{extraArgs} '
                '{configExtraArgs} '
                '"{srcDir}"'
                .format(instDir=instDir,
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


############################################################
# External dependencies required by Aurora

AllDependencies = list()
AllDependenciesByName = dict()

class Dependency(object):
    def __init__(self, name, installer, *files):
        self.name = name
        self.installer = installer
        self.filesToCheck = files

        AllDependencies.append(self)
        AllDependenciesByName.setdefault(name.lower(), self)

    def Exists(self, context):
        return all([os.path.isfile(os.path.join(context.externalsInstDir, config, f))
                    for f in self.filesToCheck for config in BuildConfigs(context)])

############################################################
# zlib

ZLIB_URL = "https://github.com/madler/zlib/archive/v1.2.11.zip"

def InstallZlib(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(ZLIB_URL, context, force)):
        RunCMake(context, force, buildArgs)

ZLIB = Dependency("zlib", InstallZlib, "include/zlib.h")

############################################################
# boost

if Linux():
    BOOST_URL = "https://boostorg.jfrog.io/artifactory/main/release/1.70.0/source/boost_1_70_0.tar.gz"
    BOOST_VERSION_FILE = "include/boost/version.hpp"
elif Windows():
    # The default installation of boost on Windows puts headers in a versioned
    # subdirectory, which we have to account for here. In theory, specifying
    # "layout=system" would make the Windows install match Linux, but that
    # causes problems for other dependencies that look for boost.
    #
    # boost 1.70 is required for Visual Studio 2019. For simplicity, we use
    # this version for all older Visual Studio versions as well.
    BOOST_URL = "https://boostorg.jfrog.io/artifactory/main/release/1.70.0/source/boost_1_70_0.tar.gz"
    BOOST_VERSION_FILE = "include/boost-1_70/boost/version.hpp"

def InstallBoost_Helper(context, force, buildArgs):
    # Documentation files in the boost archive can have exceptionally
    # long paths. This can lead to errors when extracting boost on Windows,
    # since paths are limited to 260 characters by default on that platform.
    # To avoid this, we skip extracting all documentation.
    #
    # For some examples, see: https://svn.boost.org/trac10/ticket/11677
    dontExtract = [
        "*/doc/*",
        "*/libs/*/doc/*",
        "*/libs/wave/test/testwave/testfiles/utf8-test-*"
    ]

    with CurrentWorkingDirectory(DownloadURL(BOOST_URL, context, force,
                                             dontExtract=dontExtract)):
        bootstrap = "bootstrap.bat" if Windows() else "./bootstrap.sh"
        Run(f'{bootstrap}')

        # b2 supports at most -j64 and will error if given a higher value.
        numProc = min(64, context.numJobs)

        b2Settings = [
            f'--build-dir="{context.buildDir}"',
            f'-j{numProc}',
            'address-model=64',
            'link=shared',
            'runtime-link=shared',
            'threading=multi',
            '--with-atomic',
            '--with-program_options',
            '--with-regex'
        ]

        # Required by OpenImageIO
        b2Settings.append("--with-date_time")
        b2Settings.append("--with-system")
        b2Settings.append("--with-thread")
        b2Settings.append("--with-filesystem")

        if force:
            b2Settings.append("-a")

        if Windows():
            # toolset parameter for Visual Studio documented here:
            # https://github.com/boostorg/build/blob/develop/src/tools/msvc.jam
            if context.cmakeToolset == "v142" or IsVisualStudio2019OrGreater():
                b2Settings.append("toolset=msvc-14.2")
            # elif IsVisualStudio2022OrGreater():
            #     b2Settings.append("toolset=msvc-14.x")
            else:
                b2Settings.append("toolset=msvc-14.2")

        # Add on any user-specified extra arguments.
        b2Settings += buildArgs

        b2 = "b2" if Windows() else "./b2"

        # boost only accepts three variants: debug, release, profile
        b2ExtraSettings = []
        if context.buildDebug:
            b2ExtraSettings.append('--prefix="{}" variant=debug --debug-configuration'.format(
                os.path.join(context.externalsInstDir, 'Debug')))
        if context.buildRelease:
            b2ExtraSettings.append('--prefix="{}" variant=release'.format(
                os.path.join(context.externalsInstDir, 'Release')))
        if context.buildRelWithDebInfo:
            b2ExtraSettings.append('--prefix="{}" variant=profile'.format(
                os.path.join(context.externalsInstDir, 'RelWithDebInfo')))

        for extraSettings in b2ExtraSettings:
            b2Settings.append(extraSettings)
            Run('{b2} {options} install'.format(b2=b2, options=" ".join(b2Settings)))
            b2Settings.pop()

def InstallBoost(context, force, buildArgs):
    # Boost's build system will install the version.hpp header before
    # building its libraries. We make sure to remove it in case of
    # any failure to ensure that the build script detects boost as a
    # dependency to build the next time it's run.
    try:
        InstallBoost_Helper(context, force, buildArgs)
    except:
        for config in BuildConfigs(context):
            versionHeader = os.path.join(context.externalsInstDir, config, BOOST_VERSION_FILE)
            if os.path.isfile(versionHeader):
                try: os.remove(versionHeader)
                except: pass
        raise

BOOST = Dependency("boost", InstallBoost, BOOST_VERSION_FILE)

############################################################
# Intel TBB

if Windows():
    TBB_URL = "https://github.com/oneapi-src/oneTBB/releases/download/2019_U6/tbb2019_20190410oss_win.zip"
    TBB_ROOT_DIR_NAME = "tbb2019_20190410oss"
else:
    TBB_URL = "https://github.com/oneapi-src/oneTBB/archive/refs/tags/2019_U6.tar.gz"

def InstallTBB(context, force, buildArgs):
    if Windows():
        InstallTBB_Windows(context, force, buildArgs)
    elif Linux():
        InstallTBB_Linux(context, force, buildArgs)

def InstallTBB_Windows(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TBB_URL, context, force, TBB_ROOT_DIR_NAME)):
        # On Windows, we simply copy headers and pre-built DLLs to
        # the appropriate location.
        if buildArgs:
            PrintWarning("Ignoring build arguments {}, TBB is "
                         "not built from source on this platform."
                         .format(buildArgs))

        for config in BuildConfigs(context):
            CopyFiles(context, "bin/intel64/vc14/*.*", "bin", config)
            CopyFiles(context, "lib/intel64/vc14/*.*", "lib", config)
            CopyDirectory(context, "include/serial", "include/serial", config)
            CopyDirectory(context, "include/tbb", "include/tbb", config)

def InstallTBB_Linux(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TBB_URL, context, force)):
        # TBB does not support out-of-source builds in a custom location.
        Run('make -j{procs} {buildArgs}'
            .format(procs=context.numJobs,
                    buildArgs=" ".join(buildArgs)))

        for config in BuildConfigs(context):
            if (config == "Release" or config == "RelWithDebInfo"):
                CopyFiles(context, "build/*_release/libtbb*.*", "lib", config)
            if (config == "Debug"):
                CopyFiles(context, "build/*_debug/libtbb*.*", "lib", config)
            CopyDirectory(context, "include/serial", "include/serial", config)
            CopyDirectory(context, "include/tbb", "include/tbb", config)

TBB = Dependency("TBB", InstallTBB, "include/tbb/tbb.h")

############################################################
# JPEG

JPEG_URL = "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.1.zip"

def InstallJPEG(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(JPEG_URL, context, force)):
        RunCMake(context, force, buildArgs)

JPEG = Dependency("JPEG", InstallJPEG, "include/jpeglib.h")

############################################################
# TIFF

TIFF_URL = "https://gitlab.com/libtiff/libtiff/-/archive/v4.0.7/libtiff-v4.0.7.tar.gz"

def InstallTIFF(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TIFF_URL, context, force)):
        # libTIFF has a build issue on Windows where tools/tiffgt.c
        # unconditionally includes unistd.h, which does not exist.
        # To avoid this, we patch the CMakeLists.txt to skip building
        # the tools entirely. We do this on Linux as well
        # to avoid requiring some GL and X dependencies.
        #
        # We also need to skip building tests, since they rely on
        # the tools we've just elided.
        PatchFile("CMakeLists.txt",
                   [("add_subdirectory(tools)", "# add_subdirectory(tools)"),
                    ("add_subdirectory(test)", "# add_subdirectory(test)")])

        # The libTIFF CMakeScript says the ld-version-script
        # functionality is only for compilers using GNU ld on
        # ELF systems or systems which provide an emulation; therefore
        # skipping it completely on mac and windows.
        if Windows():
            extraArgs = ["-Dld-version-script=OFF"]
        else:
            extraArgs = []
        extraArgs += buildArgs
        RunCMake(context, force, extraArgs)

TIFF = Dependency("TIFF", InstallTIFF, "include/tiff.h")

############################################################
# PNG

PNG_URL = "https://github.com/glennrp/libpng/archive/refs/tags/v1.6.29.tar.gz"

def InstallPNG(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(PNG_URL, context, force)):
        RunCMake(context, force, buildArgs)

PNG = Dependency("PNG", InstallPNG, "include/png.h")

############################################################
# GLM

GLM_URL = "https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip"

# TODO is the install structure proper?
def InstallGLM(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLM_URL, context, force)):
        for config in BuildConfigs(context):
            CopyDirectory(context, "glm", "glm", config)
            CopyDirectory(context, "cmake/glm", "cmake/glm", config)

GLM = Dependency("GLM", InstallGLM, "glm/glm.hpp")

############################################################
# STB

STB_URL = "https://github.com/nothings/stb/archive/refs/heads/master.zip"

def InstallSTB(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(STB_URL, context, force)):
        for config in BuildConfigs(context):
            CopyFiles(context, "*.h", "include", config)

STB = Dependency("STB", InstallSTB, "include/stb_image.h")

############################################################
# TinyGLTF

TinyGLTF_URL = "https://github.com/syoyo/tinygltf/archive/refs/tags/v2.5.0.zip"

def InstallTinyGLTF(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TinyGLTF_URL, context, force)):
        RunCMake(context, force, buildArgs)

TINYGLTF = Dependency("TinyGLTF", InstallTinyGLTF, "include/tiny_gltf.h")

############################################################
# TinyObjLoader

TinyObjLoader_URL = "https://github.com/tinyobjloader/tinyobjloader/archive/refs/tags/v2.0-rc1.zip"

def InstallTinyObjLoader(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TinyObjLoader_URL, context, force)):
        RunCMake(context, force, buildArgs)

TINYOBJLOADER = Dependency("TinyObjLoader", InstallTinyObjLoader, "include/tiny_obj_loader.h")

############################################################
# IlmBase/OpenEXR

if Windows():
    OPENEXR_URL = "https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v2.5.2.zip"
else:
    OPENEXR_URL = "https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v2.4.3.zip"

def InstallOpenEXR(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OPENEXR_URL, context, force)):
        extraArgs = [
            '-DPYILMBASE_ENABLE=OFF',
            '-DOPENEXR_VIEWERS_ENABLE=OFF',
            '-DBUILD_TESTING=OFF',
            '-DOPENEXR_BUILD_PYTHON_LIBS=OFF'
        ]

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        packageConfigs = {
            "Debug": '-DOPENEXR_PACKAGE_PREFIX="{}"'.format(
                os.path.join(context.externalsInstDir, 'Debug')),
            "Release": '-DOPENEXR_PACKAGE_PREFIX="{}"'.format(
                os.path.join(context.externalsInstDir, 'Release')),
            "RelWithDebInfo": '-DOPENEXR_PACKAGE_PREFIX="{}"'.format(
                os.path.join(context.externalsInstDir, 'RelWithDebInfo'))
        }

        RunCMake(context, force, extraArgs, configExtraArgs = packageConfigs)

OPENEXR = Dependency("OpenEXR", InstallOpenEXR, "include/OpenEXR/ImfVersion.h")

############################################################
# OpenImageIO

OIIO_URL = "https://github.com/OpenImageIO/oiio/archive/Release-2.1.16.0.zip"

def InstallOpenImageIO(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OIIO_URL, context, force)):
        extraArgs = ['-DOIIO_BUILD_TOOLS=OFF',
                     '-DOIIO_BUILD_TESTS=OFF',
                     '-DUSE_PYTHON=OFF',
                     '-DSTOP_ON_WARNING=OFF',
                     '-DUSE_PTEX=OFF']

        # Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        # OIIO's FindOpenEXR module circumvents CMake's normal library
        # search order, which causes versions of OpenEXR installed in
        # /usr/local or other hard-coded locations in the module to
        # take precedence over the version we've built, which would
        # normally be picked up when we specify CMAKE_PREFIX_PATH.
        # This may lead to undefined symbol errors at build or runtime.
        # So, we explicitly specify the OpenEXR we want to use here.
        openEXRConfigs = {
            "Debug": '-DOPENEXR_ROOT="{}"'.format(
                os.path.join(context.externalsInstDir, 'Debug')),
            "Release": '-DOPENEXR_ROOT="{}"'.format(
                os.path.join(context.externalsInstDir, 'Release')),
            "RelWithDebInfo": '-DOPENEXR_ROOT="{}"'.format(
                os.path.join(context.externalsInstDir, 'RelWithDebInfo')),
        }

        RunCMake(context, force, extraArgs, configExtraArgs = openEXRConfigs)

OPENIMAGEIO = Dependency("OpenImageIO", InstallOpenImageIO,
                         "include/OpenImageIO/oiioversion.h")

############################################################
# OpenSubdiv

OPENSUBDIV_URL = "https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_4_4.zip"

def InstallOpenSubdiv(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OPENSUBDIV_URL, context, force)):
        extraArgs = [
            '-DNO_EXAMPLES=ON',
            '-DNO_TUTORIALS=ON',
            '-DNO_REGRESSION=ON',
            '-DNO_DOC=ON',
            '-DNO_OMP=ON',
            '-DNO_CUDA=ON',
            '-DNO_OPENCL=ON',
            '-DNO_DX=ON',
            '-DNO_TESTS=ON',
            '-DNO_GLEW=ON',
            '-DNO_GLFW=ON',
            '-DNO_PTEX=ON',
        ]

        # NOTE: For now, we disable TBB in our OpenSubdiv build.
        # This avoids an issue where OpenSubdiv will link against
        # all TBB libraries it finds, including libtbbmalloc and
        # libtbbmalloc_proxy. On Linux, this has the
        # unwanted effect of replacing the system allocator with
        # tbbmalloc.
        extraArgs.append('-DNO_TBB=ON')

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        # OpenSubdiv seems to error when building on windows w/ Ninja...
        # ...so just use the default generator (ie, Visual Studio on Windows)
        # until someone can sort it out
        oldGenerator = context.cmakeGenerator
        if oldGenerator == "Ninja" and Windows():
            context.cmakeGenerator = None

        oldNumJobs = context.numJobs

        try:
            RunCMake(context, force, extraArgs)
        finally:
            context.cmakeGenerator = oldGenerator
            context.numJobs = oldNumJobs

OPENSUBDIV = Dependency("OpenSubdiv", InstallOpenSubdiv,
                        "include/opensubdiv/version.h")

############################################################
# MaterialX

MATERIALX_URL = "https://github.com/materialx/MaterialX/archive/v1.38.5.zip"

def InstallMaterialX(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(MATERIALX_URL, context, force)):
        cmakeOptions = ['-DMATERIALX_BUILD_SHARED_LIBS=ON']

        cmakeOptions += buildArgs

        RunCMake(context, force, cmakeOptions)

MATERIALX = Dependency("MaterialX", InstallMaterialX, "include/MaterialXCore/Library.h")

############################################################
# USD
USD_URL = "https://github.com/autodesk-forks/USD/archive/refs/tags/v22.08-Aurora-v22.11.zip"
def InstallUSD(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(USD_URL, context, force)):
# USD_URL = "https://github.com/autodesk-forks/USD.git"
# USD_TAG = "v22.08-Aurora-v22.11"
# def InstallUSD(context, force, buildArgs):
#     USD_FOLDER = "USD-"+USD_TAG
#     with CurrentWorkingDirectory(GitClone(USD_URL, USD_TAG, USD_FOLDER, context)):
        extraArgs = []

        if Linux():
            extraArgs.append('-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++')

        extraArgs.append('-DPXR_PREFER_SAFETY_OVER_SPEED=ON')
        extraArgs.append('-DBUILD_SHARED_LIBS=ON')

        extraArgs.append('-DPXR_BUILD_DOCUMENTATION=OFF')
        extraArgs.append('-DPXR_BUILD_TESTS=OFF')
        extraArgs.append('-DPXR_BUILD_EXAMPLES=OFF')
        extraArgs.append('-DPXR_BUILD_TUTORIALS=OFF')

        extraArgs.append('-DPXR_ENABLE_VULKAN_SUPPORT=ON')

        extraArgs.append('-DPXR_BUILD_USD_TOOLS=OFF')

        extraArgs.append('-DPXR_BUILD_IMAGING=ON')
        extraArgs.append('-DPXR_ENABLE_PTEX_SUPPORT=OFF')
        extraArgs.append('-DPXR_ENABLE_OPENVDB_SUPPORT=OFF')
        extraArgs.append('-DPXR_BUILD_EMBREE_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_PRMAN_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_OPENIMAGEIO_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_OPENCOLORIO_PLUGIN=OFF')

        extraArgs.append('-DPXR_BUILD_USD_IMAGING=ON')

        extraArgs.append('-DPXR_BUILD_USDVIEW=OFF')

        extraArgs.append('-DPXR_BUILD_ALEMBIC_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_DRACO_PLUGIN=OFF')

        extraArgs.append('-DPXR_ENABLE_MATERIALX_SUPPORT=OFF')

        extraArgs.append('-DPXR_ENABLE_PYTHON_SUPPORT=OFF')

        # Turn off the text system in USD (Autodesk extension)
        extraArgs.append('-DPXR_ENABLE_TEXT_SUPPORT=OFF')

        if Windows():
            # Increase the precompiled header buffer limit.
            extraArgs.append('-DCMAKE_CXX_FLAGS="/Zm150"')

        # Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        extraArgs.append('-DPXR_LIB_PREFIX=')

        extraArgs += buildArgs

        tbbConfigs = {
            "Debug": '-DTBB_USE_DEBUG_BUILD=ON',
            "Release": '-DTBB_USE_DEBUG_BUILD=OFF',
            "RelWithDebInfo": '-DTBB_USE_DEBUG_BUILD=OFF',
        }
        RunCMake(context, force, extraArgs, configExtraArgs=tbbConfigs)

USD = Dependency("USD", InstallUSD, "include/pxr/pxr.h")

############################################################
# Slang

if Windows():
    Slang_URL = "https://github.com/shader-slang/slang/releases/download/v0.24.35/slang-0.24.35-win64.zip"
else:
    Slang_URL = "https://github.com/shader-slang/slang/releases/download/v0.24.35/slang-0.24.35-linux-x86_64.zip"

def InstallSlang(context, force, buildArgs):
    Slang_FOLDER = DownloadURL(Slang_URL, context, force, destDir="Slang")
    for config in BuildConfigs(context):
        CopyDirectory(context, Slang_FOLDER, "Slang", config)

SLANG = Dependency("Slang", InstallSlang, "Slang/slang.h")

############################################################
# NRD

NRD_URL = "https://github.com/NVIDIAGameWorks/RayTracingDenoiser.git"
NRD_TAG = "v3.8.0"

def InstallNRD(context, force, buildArgs):
    NRD_FOLDER = "NRD-"+NRD_TAG
    with CurrentWorkingDirectory(GitClone(NRD_URL, NRD_TAG, NRD_FOLDER, context)):
        RunCMake(context, force, buildArgs, install=False)

        for config in BuildConfigs(context):
            CopyDirectory(context, "Include", "NRD/Include", config)
            CopyDirectory(context, "Integration", "NRD/Integration", config)
            if context.buildRelease or context.buildRelWithDebInfo :
                if Windows():
                    CopyFiles(context, "_Build/Release/*.dll", "bin", config)
                CopyDirectory(context, "_Build/Release", "NRD/Lib/Release", config)
            if context.buildDebug:
                if Windows():
                    CopyFiles(context, "_Build/Debug/*.dll", "bin", config)
                CopyDirectory(context, "_Build/Debug", "NRD/Lib/Debug", config)

            # NRD v2.x.x #TODO need to use config as part of installation path
            # CopyDirectory(context, "Shaders", "NRD/Shaders", config)
            # CopyFiles(context, "Source/Shaders/Include/*.*", "NRD/Shaders", config)
            # CopyFiles(context, "External/MathLib/*.*", "NRD/Shaders", config)
            # CopyFiles(context, "Include/*.*", "NRD/Shaders", config)

            # NRD v3.x.x
            CopyDirectory(context, "Shaders", "NRD/Shaders", config)
            CopyFiles(context, "Shaders/Include/NRD.hlsli", "NRD/Shaders/Include", config)
            CopyFiles(context, "External/MathLib/*.hlsli", "NRD/Shaders/Source", config)

NRD = Dependency("NRD", InstallNRD, "NRD/Include/NRD.h")

############################################################
# NRI

NRI_URL = "https://github.com/NVIDIAGameWorks/NRI.git"
NRI_TAG = "v1.87"

def InstallNRI(context, force, buildArgs):
    NRI_FOLDER = "NRI-"+NRI_TAG
    with CurrentWorkingDirectory(GitClone(NRI_URL, NRI_TAG, NRI_FOLDER, context)):
        RunCMake(context, force, buildArgs, install=False)

        for config in BuildConfigs(context):
            CopyDirectory(context, "Include", "NRI/Include", config)
            CopyDirectory(context, "Include/Extensions", "NRI/Include/Extensions", config)
            if context.buildRelease or context.buildRelWithDebInfo :
                if Windows():
                    CopyFiles(context, "_Build/Release/*.dll", "bin", config)
                CopyDirectory(context, "_Build/Release", "NRI/Lib/Release", config)
            if context.buildDebug:
                if Windows():
                    CopyFiles(context, "_Build/Debug/*.dll", "bin", config)
                CopyDirectory(context, "_Build/Debug", "NRI/Lib/Debug", config)

NRI = Dependency("NRI", InstallNRI, "NRI/Include/NRI.h")

############################################################
# GLEW

GLEW_URL = "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip"

def InstallGLEW(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLEW_URL, context, force)):
        for config in BuildConfigs(context):
            CopyDirectory(context, "include/GL", "include/GL", config)
            CopyFiles(context, "bin/Release/x64/*.dll", "bin", config)
            CopyFiles(context, "lib/Release/x64/*.lib", "lib", config)
            # TODO: shall we support Debug build of glew?
            # buildVariant=("Debug" if context.buildDebug or context.buildRelWithDebInfo else "Release"),
            # CopyFiles(context, f'bin/{buildVariant}/x64/*.dll', "bin")
            # CopyFiles(context, f'lib/{buildVariant}/x64/*.lib', "lib")

GLEW = Dependency("GLEW", InstallGLEW, "include/GL/glew.h")

############################################################
# GLFW

GLFW_URL = "https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip"

def InstallGLFW(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLFW_URL, context, force)):
        RunCMake(context, force, buildArgs)

GLFW = Dependency("GLFW", InstallGLFW, "include/GLFW/glfw3.h")

############################################################
# CXXOPTS

CXXOPTS_URL = "https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.zip"

def InstallCXXOPTS(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(CXXOPTS_URL, context, force)):
        RunCMake(context, force, buildArgs)

CXXOPTS = Dependency("CXXOPTS", InstallCXXOPTS, "include/cxxopts.hpp")

############################################################
# GTEST

GTEST_URL = "https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip"

def InstallGTEST(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GTEST_URL, context, force)):
        extraArgs = [*buildArgs, '-Dgtest_force_shared_crt=ON']
        RunCMake(context, force, extraArgs)

GTEST = Dependency("GTEST", InstallGTEST, "include/gtest/gtest.h")

############################################################
# Installation script

programDescription = """\
Installation Script for external libraries required by Aurora

- Libraries:
The following is a list of libraries that this script will download and build
as needed. These names can be used to identify libraries for various script
options, like --force or --build-args.

{libraryList}

- Specifying Custom Build Arguments:
Users may specify custom build arguments for libraries using the --build-args
option. This values for this option must take the form <library name>,<option>.
For example:

%(prog)s --build-args boost,cxxflags=... USD,-DPXR_STRICT_BUILD_MODE=ON ...
%(prog)s --build-args USD,"-DPXR_STRICT_BUILD_MODE=ON -DPXR_HEADLESS_TEST_MODE=ON" ...

These arguments will be passed directly to the build system for the specified
library. Multiple quotes may be needed to ensure arguments are passed on
exactly as desired. Users must ensure these arguments are suitable for the
specified library and do not conflict with other options, otherwise build
errors may occur.
""".format(
    libraryList=" ".join(sorted([d.name for d in AllDependencies])))

# Pre-requirements
if Linux():
    requiredDependenciesMsg = """
- Required dependencies:
The following libraries are required to build Aurora and its externals:
    zlib1g-dev, libjpeg-turbo8-dev, libtiff-dev, libpng-dev, libglm-dev, libglew-dev
    libglfw3-dev, libgtest-dev, libgmock-dev
You can install them with the following command on Ubuntu:
    sudo apt-get -y install zlib1g-dev libjpeg-turbo8-dev libtiff-dev libpng-dev libglm-dev libglew-dev libglfw3-dev libgtest-dev libgmock-dev
"""
else:
    requiredDependenciesMsg = ""

programDescription += requiredDependenciesMsg

parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=programDescription)

parser.add_argument("install_dir", type=str,
                    help="Directory where exetranl libraries will be installed")
parser.add_argument("-n", "--dry_run", dest="dry_run", action="store_true",
                    help="Only summarize what would happen")

group = parser.add_mutually_exclusive_group()
group.add_argument("-v", "--verbose", action="count", default=1,
                   dest="verbosity",
                   help="Increase verbosity level (1-3)")
group.add_argument("-q", "--quiet", action="store_const", const=0,
                   dest="verbosity",
                   help="Suppress all output except for error messages")

group = parser.add_argument_group(title="Build Options")
group.add_argument("--build", type=str,
                   help=("Build directory for external libraries "
                         "(default: <install_dir>/build)"))
group.add_argument("--src", type=str,
                   help=("Directory where external libraries will be downloaded "
                         "(default: <install_dir>/src)"))

# There is a cmake bug in the debug build of OpenImageIO on Linux. That bug fails the Aurora
# build if Aurora links to the debug build of OpenImageIO. Untill it is fixed by OpenImageIO,
# we will disable the build-variant option on Linux.
BUILD_DEBUG = "Debug"
BUILD_RELEASE = "Release"
BUILD_DEBUG_AND_RELEASE = "All"
BUILD_RELWITHDEBINFO  = "relwithdebuginfo"
if Windows():
    group.add_argument("--build-variant", default=BUILD_RELEASE,
                    choices=[BUILD_DEBUG, BUILD_RELEASE, BUILD_RELWITHDEBINFO, BUILD_DEBUG_AND_RELEASE],
                    help=("Build variant for external libraries. "
                            "(default: {})".format(BUILD_RELEASE)))

group.add_argument("--build-args", type=str, nargs="*", default=[],
                   help=("Custom arguments to pass to build system when "
                         "building libraries (see docs above)"))
group.add_argument("--force", type=str, action="append", dest="force_build",
                   default=[],
                   help=("Force download and build of specified library "
                         "(see docs above)"))
group.add_argument("--force-all", action="store_true",
                   help="Force download and build of all libraries")
group.add_argument("--generator", type=str,
                   help=("CMake generator to use when building libraries with "
                         "cmake"))
group.add_argument("--toolset", type=str,
                   help=("CMake toolset to use when building libraries with "
                         "cmake"))
group.add_argument("-j", "--jobs", type=int, default=GetCPUCount(),
                   help=("Number of build jobs to run in parallel. "
                         "(default: # of processors [{0}])"
                         .format(GetCPUCount())))

args = parser.parse_args()

class InstallContext:
    def __init__(self, args):
        # Assume the Aurora source directory is in the parent directory of this build script
        self.auroraSrcDir = os.path.normpath(
            os.path.join(os.path.abspath(os.path.dirname(__file__)), ".."))

        # Directory where external dependencies will be installed
        self.externalsInstDir = os.path.abspath(args.install_dir)

        # Directory where external dependencies will be downloaded and extracted
        self.externalsSrcDir = (os.path.abspath(args.src) if args.src
                                else os.path.join(self.externalsInstDir, "src"))

        # Directory where externals of Aurora will be built
        self.buildDir = (os.path.abspath(args.build) if args.build
                         else os.path.join(self.externalsInstDir, "build"))

        self.downloader = DownloadFileWithUrllib
        self.downloaderName = "built-in"

        # CMake generator and toolset
        self.cmakeGenerator = args.generator
        self.cmakeToolset = args.toolset

        # Number of jobs
        self.numJobs = args.jobs
        if self.numJobs <= 0:
            raise ValueError("Number of jobs must be greater than 0")

        # Build arguments
        self.buildArgs = dict()
        for a in args.build_args:
            (depName, _, arg) = a.partition(",")
            if not depName or not arg:
                raise ValueError("Invalid argument for --build-args: {}"
                                 .format(a))
            if depName.lower() not in AllDependenciesByName:
                raise ValueError("Invalid library for --build-args: {}"
                                 .format(depName))

            self.buildArgs.setdefault(depName.lower(), []).append(arg)

        # Build type
        if Linux():
            # There is a cmake bug in the debug build of OpenImageIO on Linux. That bug fails the Aurora
            # build if Aurora links to the debug build of OpenImageIO. Untill it is fixed by OpenImageIO,
            # we can only build release build of externals on Linux.
            self.buildDebug = False
            self.buildRelease = True
            self.buildRelWithDebInfo = False
        else:
            self.buildDebug = (args.build_variant == BUILD_DEBUG) or (args.build_variant == BUILD_DEBUG_AND_RELEASE)
            self.buildRelease = (args.build_variant == BUILD_RELEASE) or (args.build_variant == BUILD_DEBUG_AND_RELEASE)
            self.buildRelWithDebInfo  = (args.build_variant == BUILD_RELWITHDEBINFO)

        # Dependencies that are forced to be built
        self.forceBuildAll = args.force_all
        self.forceBuild = [dep.lower() for dep in args.force_build]

    def GetBuildArguments(self, dep):
        return self.buildArgs.get(dep.name.lower(), [])

    def ForceBuildDependency(self, dep):
        return self.forceBuildAll or dep.name.lower() in self.forceBuild

try:
    context = InstallContext(args)
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

verbosity = args.verbosity

# Determine list of external libraries required (directly or indirectly)
# by Aurora
requiredDependencies = [ZLIB,
                        JPEG,
                        TIFF,
                        PNG,
                        GLM,
                        STB,
                        TINYGLTF,
                        TINYOBJLOADER,
                        BOOST,
                        TBB,
                        OPENEXR,
                        OPENIMAGEIO,
                        MATERIALX,
                        OPENSUBDIV,
                        USD,
                        SLANG,
                        # Excluding NRD and NRI for Aurora 22.11 since denoiser
                        # is disabled for this release
                        # NRD,
                        # NRI,
                        GLEW,
                        GLFW,
                        CXXOPTS,
                        GTEST]

# Assume some external librraies already exist on Linux platforms and don't build
# our own. This avoids potential issues where a host application loads an older version
# of these librraies than the one we'd build and link our libraries against.
#
# On Ubuntu 20.04, you can run the following command to install these libraries:
# sudo apt-get -y install zlib1g-dev libjpeg-turbo8-dev libtiff-dev libpng-dev libglm-dev libglew-dev libglfw3-dev libgtest-dev libgmock-dev
#
# The installed libraries likely have the following versions:
# zlib1g-dev: 1.2.11
# libjpeg-turbo8-dev: 2.0.3
# libtiff-dev: 4.1.0
# libpng-dev: 1.6.37
# libglm-dev: 0.9.9.7
# libglew-dev: 2.1.0
# libglfw3-dev: 3.3.2
# libgtest-dev: 1.10.0
# libgmock-dev: 1.10.0
if Linux():
    excludes = [ZLIB, JPEG, TIFF, PNG, GLM, GLEW, GLFW, GTEST]
    for lib in excludes:
        requiredDependencies.remove(lib)
    print(requiredDependencies)

dependenciesToBuild = []
for dep in requiredDependencies:
    if context.ForceBuildDependency(dep) or not dep.Exists(context):
        if dep not in dependenciesToBuild:
            dependenciesToBuild.append(dep)

# Verify toolchain needed to build required dependencies
if (not which("g++") and
    not which("clang") and
    not GetVisualStudioCompilerAndVersion()):
    PrintError("C++ compiler not found -- please install a compiler")
    sys.exit(1)

if which("cmake"):
    # Check cmake requirements
    cmake_required_version = (3, 21)
    cmake_version = GetCMakeVersion()
    if not cmake_version:
        PrintError("Failed to determine CMake version")
        sys.exit(1)

    if cmake_version < cmake_required_version:
        def _JoinVersion(v):
            return ".".join(str(n) for n in v)
        PrintError("CMake version {req} or later required to build externals of Aurora,"
                   "but version found was {found}".format(
                       req=_JoinVersion(cmake_required_version),
                       found=_JoinVersion(cmake_version)))
        sys.exit(1)
else:
    PrintError("CMake not found -- please install it and adjust your PATH")
    sys.exit(1)

if JPEG in requiredDependencies:
    # NASM is required to build libjpeg-turbo on Windows
    if (Windows() and not which("nasm")):
        PrintError("nasm not found -- please install it and adjust your PATH")
        sys.exit(1)

Print(requiredDependenciesMsg)

# Summarize
summaryMsg = """
Building with settings:
    Aurora source directory       {auroraSrcDir}
    Externals source directory    {externalsSrcDir}
    Externals install directory   {externalsInstDir}
    Build directory               {buildDir}
    CMake generator               {cmakeGenerator}
    CMake toolset                 {cmakeToolset}
"""

summaryMsg += """
    Variant                       {buildVariant}
    Dependencies                  {dependencies}
"""

if context.buildArgs:
    summaryMsg += """
    Build arguments               {buildArgs}
"""

def FormatBuildArguments(buildArgs):
    s = ""
    for depName in sorted(buildArgs.keys()):
        args = buildArgs[depName]
        s += """
                                {name}: {args}""".format(
            name=AllDependenciesByName[depName].name,
            args=" ".join(args))
    return s.lstrip()

summaryMsg = summaryMsg.format(
    auroraSrcDir=context.auroraSrcDir,
    externalsSrcDir=context.externalsSrcDir,
    buildDir=context.buildDir,
    externalsInstDir=context.externalsInstDir,
    cmakeGenerator=("Default" if not context.cmakeGenerator
                    else context.cmakeGenerator),
    cmakeToolset=("Default" if not context.cmakeToolset
                  else context.cmakeToolset),
    dependencies=("None" if not dependenciesToBuild else
                  ", ".join([d.name for d in dependenciesToBuild])),
    buildArgs=FormatBuildArguments(context.buildArgs),
    buildVariant=("Debug and Release" if context.buildDebug and context.buildRelease
                  else "Release" if context.buildRelease
                  else "Debug" if context.buildDebug
                  else "Release w/ Debug Info" if context.buildRelWithDebInfo
                  else ""),
    )

Print(summaryMsg)

if args.dry_run:
    sys.exit(0)

# Ensure directory structure is created and is writable.
for dir in [context.externalsInstDir, context.externalsSrcDir, context.buildDir]:
    try:
        if os.path.isdir(dir):
            testFile = os.path.join(dir, "canwrite")
            open(testFile, "w").close()
            os.remove(testFile)
        else:
            os.makedirs(dir)
    except Exception as e:
        PrintError("Could not write to directory {dir}. Change permissions "
                   "or choose a different location to install to."
                   .format(dir=dir))
        sys.exit(1)

try:
    # Download, build and install external libraries
    for dep in dependenciesToBuild:
        PrintStatus("Installing {dep}...".format(dep=dep.name))
        dep.installer(context,
                      buildArgs=context.GetBuildArguments(dep),
                      force=context.ForceBuildDependency(dep))
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

if Windows():
    buildStepsMsg = """
Success!

To use the external libraries, please configure Aurora build with:
    cmake -S . -B Build -D CMAKE_BUILD_TYPE={buildVariant} -D EXTERNALS_DIR={externalsDir}
    cmake --build Build --config {buildVariant}
"""
    buildStepsMsg = buildStepsMsg.format(
        buildVariant=("Release" if context.buildRelease or context.buildRelWithDebInfo else "Debug"),
        externalsDir=context.externalsInstDir
    )
else:
    buildStepsMsg = """
Success!

To use the external libraries, you can now configure and build Aurora with:
    cmake -S . -B Build -D EXTERNALS_DIR={externalsDir}/Release
    cmake --build Build
"""
    buildStepsMsg = buildStepsMsg.format(
        externalsDir=context.externalsInstDir
    )

Print(buildStepsMsg)