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

from installExternalsFunctions import *

if sys.version_info.major < 3:
    raise Exception("Python 3 or a more recent version is required.")

############################################################
# Class representing an external dependency.
# Each instance of the  class represents a dependency, and is added to the global dependency list in the constructor,

AllDependencies = list()
AllDependenciesByName = dict()

class Dependency(object):
    def __init__(self, name, packageName, installer, versionString, *files):
        self.name = name
        self.packageName = packageName # cmake package name
        self.installer = installer
        self.installFolder = name
        self.versionString = versionString
        self.filesToCheck = files
        AllDependencies.append(self)
        AllDependenciesByName.setdefault(name.lower(), self)

    def Exists(self, context):
        return all([os.path.isfile(os.path.join(context.externalsInstDir, self.installFolder, f))
                    for f in self.filesToCheck])

    def UpdateVersion(self, context):
        UpdateVersion(context, self.packageName, self.versionString)

    def IsUpToDate(self, context):
        if(not self.Exists(context)):
            return False
        return CheckVersion(context, self.packageName, self.versionString)


############################################################
# External dependencies required by Aurora

############################################################
# zlib

ZLIB_URL = "https://github.com/madler/zlib/archive/v1.2.13.zip"
ZLIB_INSTALL_FOLDER = "zlib"
ZLIB_PACKAGE_NAME = "ZLIB"

def InstallZlib(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(ZLIB_URL, context, force)):
        RunCMake(context, True, ZLIB_INSTALL_FOLDER, buildArgs)

ZLIB = Dependency(ZLIB_INSTALL_FOLDER, ZLIB_PACKAGE_NAME, InstallZlib, ZLIB_URL, "include/zlib.h")

############################################################
# boost

BOOST_URL = "https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.gz"
# Use a sub-version in the version string to force reinstallation, even if 1.85.0 installed.
BOOST_VERSION_STRING = BOOST_URL+".a"

if Linux():
    BOOST_VERSION_FILE = "include/boost/version.hpp"
elif Windows():
    # The default installation of boost on Windows puts headers in a versioned
    # subdirectory, which we have to account for here. In theory, specifying
    # "layout=system" would make the Windows install match Linux, but that
    # causes problems for other dependencies that look for boost.
    BOOST_VERSION_FILE = "include/boost-1_85/boost/version.hpp"

BOOST_INSTALL_FOLDER = "boost"
BOOST_PACKAGE_NAME = "Boost"

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
    
        # Remove the install folder if it exists.
        instFolder = os.path.join(context.externalsInstDir, BOOST_INSTALL_FOLDER)
        if(os.path.isdir(instFolder)):
            PrintInfo("Removing existing install folder:"+instFolder)
            shutil.rmtree(instFolder)
        
        # Set the bootstrap toolset 
        bsToolset = ""
        if Windows():
            if context.cmakeToolset == "v143" or IsVisualStudio2022OrGreater():
                bsToolset = "vc143"
            elif context.cmakeToolset == "v142" or IsVisualStudio2019OrGreater():
                bsToolset = "vc142"

        bootstrap = "bootstrap.bat" if Windows() else "./bootstrap.sh"
        Run(f'{bootstrap} {bsToolset}')

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

        # Required by USD built with python support
        b2Settings.append("--with-python")
        pythonInfo = GetPythonInfo(context)
        # This is the only platform-independent way to configure these
        # settings correctly and robustly for the Boost jam build system.
        # There are Python config arguments that can be passed to bootstrap
        # but those are not available in boostrap.bat (Windows) so we must
        # take the following approach:
        projectPath = 'python-config.jam'
        with open(projectPath, 'w') as projectFile:
            # Note that we must escape any special characters, like
            # backslashes for jam, hence the mods below for the path
            # arguments. Also, if the path contains spaces jam will not
            # handle them well. Surround the path parameters in quotes.
            projectFile.write('using python : %s\n' % pythonInfo[3])
            projectFile.write('  : "%s"\n' % pythonInfo[0].replace("\\","/"))
            projectFile.write('  : "%s"\n' % pythonInfo[2].replace("\\","/"))
            projectFile.write('  : "%s"\n' % os.path.dirname(pythonInfo[1]).replace("\\","/"))
            projectFile.write('  ;\n')
        b2Settings.append("--user-config=python-config.jam")

        # Required by OpenImageIO
        b2Settings.append("--with-date_time")
        b2Settings.append("--with-chrono")
        b2Settings.append("--with-system")
        b2Settings.append("--with-thread")
        b2Settings.append("--with-filesystem")

        if force:
            b2Settings.append("-a")

        if Windows():
            # toolset parameter for Visual Studio documented here:
            # https://github.com/boostorg/build/blob/develop/src/tools/msvc.jam
            if context.cmakeToolset == "v143" or IsVisualStudio2022OrGreater():
                b2Settings.append("toolset=msvc-14.3")
            elif context.cmakeToolset == "v142" or IsVisualStudio2019OrGreater():
                 b2Settings.append("toolset=msvc-14.2")
            else:
                b2Settings.append("toolset=msvc-14.2")

        # Add on any user-specified extra arguments.
        b2Settings += buildArgs

        b2 = "b2" if Windows() else "./b2"

        # boost only accepts three variants: debug, release, profile
        b2ExtraSettings = []
        if context.buildDebug:
            b2ExtraSettings.append('--prefix="{}" variant=debug --debug-configuration'.format(
                instFolder))
        if context.buildRelease:
            b2ExtraSettings.append('--prefix="{}" variant=release'.format(
                instFolder))
        if context.buildRelWithDebInfo:
            b2ExtraSettings.append('--prefix="{}" variant=profile'.format(
                instFolder))
        for extraSettings in b2ExtraSettings:
            b2Settings.append(extraSettings)
            # Build and install Boost
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

BOOST = Dependency(BOOST_INSTALL_FOLDER, BOOST_PACKAGE_NAME, InstallBoost, BOOST_VERSION_STRING, BOOST_VERSION_FILE)

############################################################
# Intel TBB

if Windows():
    TBB_URL = "https://github.com/oneapi-src/oneTBB/releases/download/v2020.3/tbb-2020.3-win.zip"
    TBB_ROOT_DIR_NAME = "tbb"
else:
    # Use point release with fix https://github.com/oneapi-src/oneTBB/pull/833
    TBB_URL = "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2020.3.1.zip"

TBB_INSTALL_FOLDER = "tbb"
TBB_PACKAGE_NAME = "TBB"

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

        CopyFiles(context, "bin/intel64/vc14/*.*", "bin", TBB_INSTALL_FOLDER)
        CopyFiles(context, "lib/intel64/vc14/*.*", "lib", TBB_INSTALL_FOLDER)
        CopyDirectory(context, "include/serial", "include/serial", TBB_INSTALL_FOLDER)
        CopyDirectory(context, "include/tbb", "include/tbb", TBB_INSTALL_FOLDER)

def InstallTBB_Linux(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TBB_URL, context, force)):
        # TBB does not support out-of-source builds in a custom location.
        Run('make -j{procs} {buildArgs}'
            .format(procs=context.numJobs,
                    buildArgs=" ".join(buildArgs)))

        for config in BuildConfigs(context):
            if (config == "Debug"):
                CopyFiles(context, "build/*_debug/libtbb*.2", "lib", TBB_INSTALL_FOLDER)
            if (config == "Release" or config == "RelWithDebInfo"):
                CopyFiles(context, "build/*_release/libtbb*.2", "lib", TBB_INSTALL_FOLDER)
            installLibDir = os.path.join(context.externalsInstDir, TBB_INSTALL_FOLDER, "lib")
            with CurrentWorkingDirectory(installLibDir):
                MakeSymLink(context, f"*.2")
            CopyDirectory(context, "include/serial", "include/serial", TBB_INSTALL_FOLDER)
            CopyDirectory(context, "include/tbb", "include/tbb", TBB_INSTALL_FOLDER)

TBB = Dependency(TBB_INSTALL_FOLDER, TBB_PACKAGE_NAME, InstallTBB, TBB_URL, "include/tbb/tbb.h")

############################################################
# JPEG

JPEG_URL = "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.1.zip"
JPEG_INSTALL_FOLDER = "libjpeg"
JPEG_PACKAGE_NAME = "JPEG"

def InstallJPEG(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(JPEG_URL, context, force)):
        RunCMake(context, True, JPEG_INSTALL_FOLDER, buildArgs)

JPEG = Dependency(JPEG_INSTALL_FOLDER, JPEG_PACKAGE_NAME, InstallJPEG, JPEG_URL, "include/jpeglib.h")

############################################################
# TIFF

TIFF_URL = "https://gitlab.com/libtiff/libtiff/-/archive/v4.0.7/libtiff-v4.0.7.tar.gz"
TIFF_INSTALL_FOLDER = "libtiff"
TIFF_PACKAGE_NAME = "TIFF"

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
        RunCMake(context, True, TIFF_INSTALL_FOLDER, extraArgs)

TIFF = Dependency(TIFF_INSTALL_FOLDER, TIFF_PACKAGE_NAME, InstallTIFF, TIFF_URL, "include/tiff.h")

############################################################
# PNG

PNG_URL = "https://github.com/glennrp/libpng/archive/refs/tags/v1.6.38.zip"
PNG_INSTALL_FOLDER = "libpng"
PNG_PACKAGE_NAME = "PNG"

def InstallPNG(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(PNG_URL, context, force)):
        RunCMake(context, True, PNG_INSTALL_FOLDER, buildArgs)

PNG = Dependency(PNG_INSTALL_FOLDER, PNG_PACKAGE_NAME, InstallPNG, PNG_URL, "include/png.h")

############################################################
# GLM

GLM_URL = "https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip"
GLM_INSTALL_FOLDER = "glm"
GLM_PACKAGE_NAME = "glm"

def InstallGLM(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLM_URL, context, force)):
        CopyDirectory(context, "glm", "glm", GLM_INSTALL_FOLDER)

GLM = Dependency(GLM_INSTALL_FOLDER, GLM_PACKAGE_NAME, InstallGLM, GLM_URL, "glm/glm.hpp")

############################################################
# STB

STB_URL = "https://github.com/nothings/stb.git"
STB_SHA = "013ac3beddff3dbffafd5177e7972067cd2b5083" # master on 2024-06-01
STB_INSTALL_FOLDER = "stb"
STB_PACKAGE_NAME = "stb"

def InstallSTB(context, force, buildArgs):
    STB_FOLDER="stb-"+STB_SHA
    with CurrentWorkingDirectory(GitCloneSHA(STB_URL, STB_SHA, STB_FOLDER, context)):
        CopyFiles(context, "*.h", "include", STB_INSTALL_FOLDER)

STB = Dependency(STB_INSTALL_FOLDER, STB_PACKAGE_NAME, InstallSTB, STB_SHA, "include/stb_image.h")

############################################################
# TinyGLTF

TinyGLTF_URL = "https://github.com/syoyo/tinygltf/archive/refs/tags/v2.9.2.zip"
TinyGLTF_INSTALL_FOLDER = "tinygltf"
TinyGLTF_PACKAGE_NAME = "TinyGLTF"

def InstallTinyGLTF(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TinyGLTF_URL, context, force)):
        RunCMake(context, True, TinyGLTF_INSTALL_FOLDER, buildArgs)

TINYGLTF = Dependency(TinyGLTF_INSTALL_FOLDER, TinyGLTF_PACKAGE_NAME, InstallTinyGLTF, TinyGLTF_URL, "include/tiny_gltf.h")

############################################################
# TinyObjLoader

TinyObjLoader_URL = "https://github.com/tinyobjloader/tinyobjloader/archive/refs/tags/v2.0-rc1.zip"
TinyObjLoader_INSTALL_FOLDER = "tinyobjloader"
TinyObjLoader_PACKAGE_NAME = "tinyobjloader"

def InstallTinyObjLoader(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TinyObjLoader_URL, context, force)):
        RunCMake(context, True, TinyObjLoader_INSTALL_FOLDER, buildArgs)

TINYOBJLOADER = Dependency(TinyObjLoader_INSTALL_FOLDER, TinyObjLoader_PACKAGE_NAME, InstallTinyObjLoader, TinyObjLoader_URL, "include/tiny_obj_loader.h")

############################################################
# TinyEXR

TinyEXR_URL = "https://github.com/syoyo/tinyexr/archive/refs/tags/v1.0.8.zip"
TinyEXR_INSTALL_FOLDER = "tinyexr"
TinyEXR_PACKAGE_NAME = "tinyexr"
TinyEXR_INSTALL_FOLDER = "tinyexr"

def InstallTinyEXR(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(TinyEXR_URL, context, force)):
        CopyFiles(context, "tinyexr.h", "include", TinyEXR_INSTALL_FOLDER)

TINYEXR = Dependency(TinyEXR_INSTALL_FOLDER, TinyEXR_PACKAGE_NAME, InstallTinyEXR, TinyEXR_URL, "include/tinyexr.h")

############################################################
# miniz

miniz_URL = "https://github.com/richgel999/miniz/archive/refs/tags/3.0.2.zip"

miniz_INSTALL_FOLDER = "miniz"
miniz_PACKAGE_NAME = "miniz"

def InstallMiniZ(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(miniz_URL, context, force)):
        extraArgs = ['-DCMAKE_POSITION_INDEPENDENT_CODE=ON']

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs
        RunCMake(context, True, miniz_INSTALL_FOLDER, extraArgs)

MINIZ = Dependency(miniz_INSTALL_FOLDER, miniz_PACKAGE_NAME, InstallMiniZ, miniz_URL, "include/miniz/miniz_export.h")

############################################################
# uriparser

URIPARSER_URL = "https://codeload.github.com/uriparser/uriparser/tar.gz/refs/tags/uriparser-0.9.7"

URIPARSER_INSTALL_FOLDER = "uriparser"
URIPARSER_PACKAGE_NAME = "uriparser"

def InstallURIParser(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(URIPARSER_URL, context, force)):

        extraArgs = ['-DURIPARSER_BUILD_TESTS=OFF ',
                     '-DURIPARSER_BUILD_DOCS=OFF ',
                     '-DBUILD_SHARED_LIBS=OFF ',
                     '-DCMAKE_POSITION_INDEPENDENT_CODE=ON'
                    ]

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs
        RunCMake(context, True, URIPARSER_INSTALL_FOLDER, extraArgs)

URIPARSER = Dependency(URIPARSER_INSTALL_FOLDER, URIPARSER_PACKAGE_NAME, InstallURIParser, URIPARSER_URL, "bin/uriparse.exe")

############################################################
# IlmBase/OpenEXR

OPENEXR_URL = "https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v3.1.11.zip"

OPENEXR_INSTALL_FOLDER = "OpenEXR"
OPENEXR_PACKAGE_NAME = "OpenEXR"

def InstallOpenEXR(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OPENEXR_URL, context, force)):
        extraArgs = [
            '-DPYILMBASE_ENABLE=OFF',
            '-DOPENEXR_VIEWERS_ENABLE=OFF',
            '-DBUILD_TESTING=OFF',
            '-DOPENEXR_BUILD_PYTHON_LIBS=OFF',
            '-DOPENEXR_PACKAGE_PREFIX="{}"'.format(
                os.path.join(context.externalsInstDir, OPENEXR_INSTALL_FOLDER))
        ]

        # Add on any user-specified extra arguments.
        extraArgs += buildArgs

        RunCMake(context, True, OPENEXR_INSTALL_FOLDER, extraArgs)

OPENEXR = Dependency(OPENEXR_INSTALL_FOLDER, OPENEXR_PACKAGE_NAME, InstallOpenEXR, OPENEXR_URL, "include/OpenEXR/ImfVersion.h")

############################################################
# OpenImageIO

# Update to a newer version than the one adopted by OpenUSD to avoid using deprecated boost methods.
OIIO_URL = "https://github.com/OpenImageIO/oiio/archive/refs/tags/v2.5.11.0.zip"

OIIO_INSTALL_FOLDER = "OpenImageIO"
OIIO_PACKAGE_NAME = "OpenImageIO"

def InstallOpenImageIO(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(OIIO_URL, context, force)):
        ApplyGitPatch(context, "OpenImageIO.patch")

        extraArgs = ['-DOIIO_BUILD_TOOLS=OFF',
                     '-DOIIO_BUILD_TESTS=OFF',
                     '-DBUILD_DOCS=OFF',
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
        extraArgs.append('-DOPENEXR_ROOT="{}"'.format(
                os.path.join(context.externalsInstDir, OPENEXR_INSTALL_FOLDER)))

        tbbConfigs = {
            "Debug": '-DTBB_USE_DEBUG_BUILD=ON',
            "Release": '-DTBB_USE_DEBUG_BUILD=OFF',
            "RelWithDebInfo": '-DTBB_USE_DEBUG_BUILD=OFF',
        }

        RunCMake(context, True, OIIO_INSTALL_FOLDER, extraArgs, configExtraArgs=tbbConfigs)

OPENIMAGEIO = Dependency(OIIO_INSTALL_FOLDER, OIIO_PACKAGE_NAME, InstallOpenImageIO, OIIO_URL, "include/OpenImageIO/oiioversion.h")

############################################################
# OpenSubdiv

OPENSUBDIV_URL = "https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_6_0.zip"
OPENSUBDIV_INSTALL_FOLDER = "OpenSubdiv"
OPENSUBDIV_PACKAGE_NAME = "OpenSubdiv"

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
            RunCMake(context, True, OPENSUBDIV_INSTALL_FOLDER, extraArgs)
        finally:
            context.cmakeGenerator = oldGenerator
            context.numJobs = oldNumJobs

OPENSUBDIV = Dependency(OPENSUBDIV_INSTALL_FOLDER, OPENSUBDIV_PACKAGE_NAME, InstallOpenSubdiv, OPENSUBDIV_URL,
                        "include/opensubdiv/version.h")

############################################################
# MaterialX

MATERIALX_URL = "https://github.com/AcademySoftwareFoundation/MaterialX/archive/v1.38.10.zip"
MATERIALX_INSTALL_FOLDER = "MaterialX"
MATERIALX_PACKAGE_NAME = "MaterialX"

def InstallMaterialX(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(MATERIALX_URL, context, force)):
        cmakeOptions = ['-DMATERIALX_BUILD_SHARED_LIBS=ON', '-DMATERIALX_BUILD_TESTS=OFF']
        cmakeOptions += buildArgs

        RunCMake(context, True, MATERIALX_INSTALL_FOLDER, cmakeOptions)

MATERIALX = Dependency(MATERIALX_INSTALL_FOLDER, MATERIALX_PACKAGE_NAME, InstallMaterialX, MATERIALX_URL, "include/MaterialXCore/Library.h")

############################################################
# USD
USD_URL = "https://github.com/autodesk-forks/USD/archive/refs/tags/v24.08-Aurora-v24.08.zip"
USD_INSTALL_FOLDER = "USD"
USD_PACKAGE_NAME = "pxr"

def InstallUSD(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(USD_URL, context, force)):

# USD_URL = "https://github.com/autodesk-forks/USD.git"
# USD_TAG = "v24.08-Aurora-v24.08"
# USD_FOLDER = "USD-" + USD_TAG
# USD_INSTALL_FOLDER = "USD"
# USD_PACKAGE_NAME = "pxr"

# def InstallUSD(context, force, buildArgs):
#     with CurrentWorkingDirectory(GitClone(USD_URL, USD_TAG, USD_FOLDER, context)):

        # We need to apply patch to make USD build with our externals configuration
        # ApplyGitPatch(context, "USD.patch")

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

        extraArgs.append('-DPXR_BUILD_USD_TOOLS=ON')

        extraArgs.append('-DPXR_BUILD_IMAGING=ON')
        extraArgs.append('-DPXR_ENABLE_PTEX_SUPPORT=OFF')
        extraArgs.append('-DPXR_ENABLE_OPENVDB_SUPPORT=OFF')
        extraArgs.append('-DPXR_BUILD_EMBREE_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_PRMAN_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_OPENIMAGEIO_PLUGIN=ON')
        extraArgs.append('-DPXR_BUILD_OPENCOLORIO_PLUGIN=OFF')

        extraArgs.append('-DPXR_BUILD_USD_IMAGING=ON')

        extraArgs.append('-DPXR_BUILD_USDVIEW=ON')

        extraArgs.append('-DPXR_BUILD_ALEMBIC_PLUGIN=OFF')
        extraArgs.append('-DPXR_BUILD_DRACO_PLUGIN=OFF')

        extraArgs.append('-DPXR_ENABLE_MATERIALX_SUPPORT=OFF')

        # Turn off the text system in USD (Autodesk extension)
        extraArgs.append('-DPXR_ENABLE_TEXT_SUPPORT=OFF')

        extraArgs.append('-DPXR_ENABLE_PYTHON_SUPPORT=ON')
        extraArgs.append('-DPXR_USE_PYTHON_3=ON')
        pythonInfo = GetPythonInfo(context)
        if pythonInfo:
            # According to FindPythonLibs.cmake these are the variables
            # to set to specify which Python installation to use.
            extraArgs.append('-DPYTHON_EXECUTABLE="{pyExecPath}"'
                                .format(pyExecPath=pythonInfo[0]))
            extraArgs.append('-DPYTHON_LIBRARY="{pyLibPath}"'
                                .format(pyLibPath=pythonInfo[1]))
            extraArgs.append('-DPYTHON_INCLUDE_DIR="{pyIncPath}"'
                                .format(pyIncPath=pythonInfo[2]))
            extraArgs.append('-DPXR_USE_DEBUG_PYTHON=OFF')

        if Windows():
            # Increase the precompiled header buffer limit.
            extraArgs.append('-DCMAKE_CXX_FLAGS="/Zm150"')

        # Make sure to use boost installed by the build script and not any
        # system installed boost
        extraArgs.append('-DBoost_NO_BOOST_CMAKE=On')
        extraArgs.append('-DBoost_NO_SYSTEM_PATHS=True')

        extraArgs.append('-DPXR_LIB_PREFIX=')

        extraArgs += buildArgs

        # extraArgs.append('-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=TRUE')

        tbbConfigs = {
            "Debug": '-DTBB_USE_DEBUG_BUILD=ON',
            "Release": '-DTBB_USE_DEBUG_BUILD=OFF',
            "RelWithDebInfo": '-DTBB_USE_DEBUG_BUILD=OFF',
        }
        RunCMake(context, True, USD_INSTALL_FOLDER, extraArgs, configExtraArgs=tbbConfigs)

USD = Dependency(USD_INSTALL_FOLDER, USD_PACKAGE_NAME, InstallUSD, USD_URL, "include/pxr/pxr.h")

############################################################
# Slang

if Windows():
    Slang_URL = "https://github.com/shader-slang/slang/releases/download/v0.24.35/slang-0.24.35-win64.zip"
else:
    Slang_URL = "https://github.com/shader-slang/slang/releases/download/v0.24.35/slang-0.24.35-linux-x86_64.zip"
Slang_INSTALL_FOLDER = "Slang"
Slang_PACKAGE_NAME = "Slang"

def InstallSlang(context, force, buildArgs):
    Slang_SRC_FOLDER = DownloadURL(Slang_URL, context, force, destDir="Slang")
    CopyDirectory(context, Slang_SRC_FOLDER, Slang_INSTALL_FOLDER)

SLANG = Dependency(Slang_INSTALL_FOLDER, Slang_PACKAGE_NAME, InstallSlang, Slang_URL, "slang.h")

############################################################
# NRD

NRD_URL = "https://github.com/NVIDIAGameWorks/RayTracingDenoiser.git"
NRD_TAG = "v3.8.0"
NRD_INSTALL_FOLDER = "NRD"
NRD_PACKAGE_NAME = "NRD"

def InstallNRD(context, force, buildArgs):
    NRD_FOLDER = "NRD-"+NRD_TAG
    with CurrentWorkingDirectory(GitClone(NRD_URL, NRD_TAG, NRD_FOLDER, context)):
        RunCMake(context, True, NRD_INSTALL_FOLDER, buildArgs, install=False)

        CopyDirectory(context, "Include", "include", NRD_INSTALL_FOLDER)
        CopyDirectory(context, "Integration", "Integration", NRD_INSTALL_FOLDER)
        if context.buildRelease or context.buildRelWithDebInfo :
            if Windows():
                CopyFiles(context, "_Build/Release/*.dll", "bin", NRD_INSTALL_FOLDER)
            CopyFiles(context, "_Build/Release/*", "lib", NRD_INSTALL_FOLDER)
        if context.buildDebug:
            if Windows():
                CopyFiles(context, "_Build/Debug/*.dll", "bin", NRD_INSTALL_FOLDER)
            CopyFiles(context, "_Build/Debug/*", "lib", NRD_INSTALL_FOLDER)

        # NRD v3.x.x
        CopyDirectory(context, "Shaders", "Shaders", NRD_INSTALL_FOLDER)
        CopyFiles(context, "Shaders/Include/NRD.hlsli", "Shaders/Include", NRD_INSTALL_FOLDER)
        CopyFiles(context, "External/MathLib/*.hlsli", "Shaders/Source", NRD_INSTALL_FOLDER)

NRD = Dependency(NRD_INSTALL_FOLDER, NRD_PACKAGE_NAME, InstallNRD, NRD_URL, "include/NRD.h")

############################################################
# NRI

NRI_URL = "https://github.com/NVIDIAGameWorks/NRI.git"
NRI_TAG = "v1.87"
NRI_INSTALL_FOLDER = "NRI"
NRI_PACKAGE_NAME = "NRI"

def InstallNRI(context, force, buildArgs):
    NRI_FOLDER = "NRI-"+NRI_TAG
    with CurrentWorkingDirectory(GitClone(NRI_URL, NRI_TAG, NRI_FOLDER, context)):
        RunCMake(context, force, NRI_INSTALL_FOLDER, buildArgs, install=False)

        CopyDirectory(context, "Include", "include", NRI_INSTALL_FOLDER)
        CopyDirectory(context, "Include/Extensions", "include/Extensions", NRI_INSTALL_FOLDER)
        if context.buildRelease or context.buildRelWithDebInfo :
            if Windows():
                CopyFiles(context, "_Build/Release/*.dll", "bin", NRI_INSTALL_FOLDER)
            CopyFiles(context, "_Build/Release/*", "lib", NRI_INSTALL_FOLDER)
        if context.buildDebug:
            if Windows():
                CopyFiles(context, "_Build/Debug/*.dll", "bin", NRI_INSTALL_FOLDER)
            CopyFiles(context, "_Build/Debug/*", "lib", NRI_INSTALL_FOLDER)

NRI = Dependency(NRI_INSTALL_FOLDER, NRI_PACKAGE_NAME, InstallNRI, NRI_URL, "include/NRI.h")

############################################################
# GLEW

GLEW_URL = "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip"
GLEW_INSTALL_FOLDER = "glew"
GLEW_PACKAGE_NAME = "GLEW"

def InstallGLEW(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLEW_URL, context, force)):
        CopyDirectory(context, "include/GL", "include/GL", GLEW_INSTALL_FOLDER)
        CopyFiles(context, "bin/Release/x64/*.dll", "bin", GLEW_INSTALL_FOLDER)
        CopyFiles(context, "lib/Release/x64/*.lib", "lib", GLEW_INSTALL_FOLDER)

GLEW = Dependency(GLEW_INSTALL_FOLDER, GLEW_PACKAGE_NAME, InstallGLEW, GLEW_URL, "include/GL/glew.h")

############################################################
# GLFW

GLFW_URL = "https://github.com/glfw/glfw/archive/refs/tags/3.3.8.zip"
GLFW_INSTALL_FOLDER = "GLFW"
GLFW_PACKAGE_NAME = "glfw3"

def InstallGLFW(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GLFW_URL, context, force)):
        cmakeOptions = ['-DGLFW_BUILD_EXAMPLES=OFF', '-DGLFW_BUILD_TESTS=OFF', '-DGLFW_BUILD_DOCS=OFF']
        cmakeOptions += buildArgs

        RunCMake(context, True, GLFW_INSTALL_FOLDER, cmakeOptions)

GLFW = Dependency(GLFW_INSTALL_FOLDER, GLFW_PACKAGE_NAME, InstallGLFW, GLFW_URL, "include/GLFW/glfw3.h")

############################################################
# CXXOPTS

CXXOPTS_URL = "https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.zip"
CXXOPTS_INSTALL_FOLDER = "cxxopts"
CXXOPTS_PACKAGE_NAME = "cxxopts"

def InstallCXXOPTS(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(CXXOPTS_URL, context, force)):
        RunCMake(context, True, CXXOPTS_INSTALL_FOLDER, buildArgs)

CXXOPTS = Dependency(CXXOPTS_INSTALL_FOLDER, CXXOPTS_PACKAGE_NAME, InstallCXXOPTS, CXXOPTS_URL, "include/cxxopts.hpp")

############################################################
# GTEST

GTEST_URL = "https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip"
GTEST_INSTALL_FOLDER = "gtest"
GTEST_PACKAGE_NAME = "GTest"

def InstallGTEST(context, force, buildArgs):
    with CurrentWorkingDirectory(DownloadURL(GTEST_URL, context, force)):
        extraArgs = [*buildArgs, '-Dgtest_force_shared_crt=ON']
        RunCMake(context, True, GTEST_INSTALL_FOLDER, extraArgs)

GTEST = Dependency(GTEST_INSTALL_FOLDER, GTEST_PACKAGE_NAME, InstallGTEST, GTEST_URL, "include/gtest/gtest.h")

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
BUILD_RELWITHDEBINFO  = "RelWithDebInfo"
group.add_argument("--build-variant", default=BUILD_RELEASE,
                choices=[BUILD_DEBUG, BUILD_RELEASE, BUILD_DEBUG_AND_RELEASE],
                help=("Build variant for external libraries. "
                        "(default: {})".format(BUILD_RELEASE)))

group.add_argument("--build-args", type=str, nargs="*", default=[],
                   help=("Custom arguments to pass to build system when "
                         "building libraries (see docs above)"))
group.add_argument("--build-python-info", type=str, nargs=4, default=[],
                   metavar=('PYTHON_EXECUTABLE', 'PYTHON_INCLUDE_DIR', 'PYTHON_LIBRARY', 'PYTHON_VERSION'),
                   help=("Specify a custom python to use during build"))
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

        self.buildConfigs = []

        # Build python info
        self.build_python_info = dict()
        if args.build_python_info:
            self.build_python_info['PYTHON_EXECUTABLE'] = args.build_python_info[0]
            self.build_python_info['PYTHON_INCLUDE_DIR'] = args.build_python_info[1]
            self.build_python_info['PYTHON_LIBRARY'] = args.build_python_info[2]
            self.build_python_info['PYTHON_VERSION'] = args.build_python_info[3]

        # Build type
        self.buildDebug = (args.build_variant == BUILD_DEBUG) or (args.build_variant == BUILD_DEBUG_AND_RELEASE)
        self.buildRelease = (args.build_variant == BUILD_RELEASE) or (args.build_variant == BUILD_DEBUG_AND_RELEASE)
        self.buildRelWithDebInfo  = (args.build_variant == BUILD_RELWITHDEBINFO)

        if self.buildRelease:
            self.buildConfigs.append("Release")
        if self.buildDebug:
            self.buildConfigs.append("Debug")
        if self.buildRelWithDebInfo:
            self.buildConfigs.append("RelWithDebInfo")

        # Dependencies that are forced to be built
        self.forceBuildAll = args.force_all
        self.forceBuild = [dep.lower() for dep in args.force_build]

        self.cmakePrefixPaths = set()

    def GetBuildArguments(self, dep):
        return self.buildArgs.get(dep.name.lower(), [])

    def ForceBuildDependency(self, dep):
        return self.forceBuildAll or dep.name.lower() in self.forceBuild

try:
    context = InstallContext(args)
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

SetVerbosity(args.verbosity)

# Determine list of external libraries required (directly or indirectly)
# by Aurora
requiredDependencies = [ZLIB,
                        JPEG,
                        TIFF,
                        PNG,
                        GLM,
                        STB,
                        MINIZ,
                        URIPARSER,
                        TINYGLTF,
                        TINYOBJLOADER,
                        TINYEXR,
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

context.cmakePrefixPaths = set(map(lambda lib: os.path.join(context.externalsInstDir, lib.installFolder), requiredDependencies))

dependenciesToBuild = []
for dep in requiredDependencies:
    if context.ForceBuildDependency(dep) or not dep.IsUpToDate(context):
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
    All dependencies              {allDependencies}
    Dependencies to install       {dependencies}
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
    allDependencies=("None" if not AllDependencies else
                  ", ".join([d.name for d in AllDependencies])),
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
        dep.UpdateVersion(context)
except Exception as e:
    PrintError(str(e))
    sys.exit(1)

WriteExternalsConfig(context, requiredDependencies)

if Windows():
    buildStepsMsg = """
Success!

To use the external libraries, please configure Aurora build in "x64 Native Tools Command Prompt" with:
    cmake -S . -B Build [-D EXTERNALS_ROOT={externalsDir}]
    cmake --build Build --config {buildConfigs}
""".format(externalsDir=context.externalsInstDir,
           buildConfigs="|".join(context.buildConfigs))
else:
    buildStepsMsg = """
Success!

To use the external libraries, you can now configure and build Aurora with:
    cmake -S . -B Build [-D CMAKE_BUILD_TYPE=Release|Debug] [-D EXTERNALS_ROOT={externalsDir}]
    cmake --build Build
""".format(externalsDir=context.externalsInstDir)

Print(buildStepsMsg)
