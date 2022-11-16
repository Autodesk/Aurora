# Aurora
**Aurora** is a real-time path tracing renderer that leverages GPU hardware ray tracing. As a real-time renderer, it is intended to support rapid design iteration in a real-time viewport, which differs from a "final frame" production renderer like [Autodesk Arnold](https://www.arnoldrenderer.com). Aurora has a USD Hydra render delegate called [HdAurora](Doc/HdAurora.md), which allows it to be used from a USD Hydra scene delegate. It can also be used directly through its own API, which is demonstrated with a standalone sample application called [Plasma](Doc/Plasma.md).

Aurora is developed and maintained by Autodesk. The software and this documentation are a work-in-progress and under active development. The contents of this repository are fully open source under [the Apache license](LICENSE.md), with [feature requests and code contributions](CONTRIBUTING.md) welcome!

Below you can learn about features, system requirements, how to build Aurora, how to run it, and access additional documentation.

<img src="Doc/sample.jpg" alt="Sample screenshot" style="zoom:50%;" />

<p align="center"><i>Screenshots of <a href="#sample-data">the Autodesk Telescope model</a> rendered with Aurora. Model courtesy of <a href="https://robertoziche.com">Roberto Ziche</a>.</i></p>

## Features

- Path tracing and the global effects that come with it: soft shadows, reflections, refractions, bounced light, and more.
- Interactive performance for complex scenes, using hardware ray tracing in modern GPUs.
- [Autodesk Standard Surface](https://autodesk.github.io/standard-surface) materials defined with [MaterialX](https://materialx.org) documents, which can represent a wide variety of real-world materials with physically-based shading. Also, independent layers of materials are supported, which can be used to implement decals.
- Environment lighting with a wrap-around lat-long image.
- Triangle geometry with object instancing.
- A USD Hydra render delegate (HdAurora) and standalone sample application (Plasma).

... with new features and enhancements to performance and quality planned. This will include denoising with [NVIDIA Real-Time Denoisers](https://developer.nvidia.com/rtx/ray-tracing/rt-denoisers), support for alternative material models, discrete light sources, and more.

## System Requirements

### Operating System

Aurora is officially supported on **Windows 10** or **Ubuntu 20.04**. Windows 11 and other Linux distributions may work, but are not yet supported.

To run Aurora, the latest GPU drivers from [NVIDIA](https://www.nvidia.com/download/index.aspx), [AMD](https://www.amd.com/en/support), or [Intel](https://www.intel.com/content/www/us/en/download-center/home.html) are recommended as ray tracing API support is being actively improved. No other software is required to run Aurora.

### Build Software

The following software is required to build Aurora:

- **C++ Compiler:** [Microsoft Visual Studio 2019](https://visualstudio.microsoft.com/vs/older-downloads) or [Clang 11](https://releases.llvm.org). Newer versions may work, but are not yet supported.
- **Git** (any recent version) from [the Git web site](https://git-scm.com/downloads).
- **CMake** version 3.21 or later, from [the CMake web site](https://cmake.org/download).
- **Python** version 3.7 or later, from [the Python web site](https://www.python.org/downloads).
- **The Vulkan SDK**, which can be downloaded from [LunarG's Vulkan SDK web site](https://vulkan.lunarg.com/sdk/home).

#### Windows

The following software is also required for building on Windows:

- **Windows SDK** version 10.0.22000.194 or later, for Windows builds. This can be installed using the Visual Studio Installer, or with a package from the [Microsoft web site](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk). This version is needed for a more recent build of the DirectX Shader Compiler (DXC). Note that the SDK is called "Windows SDK for Windows 11" but it supports Windows 10 as well.
- **Netwide Assembler (NASM)**, from [the NASM web site](https://www.nasm.us). This is needed to build the JPEG library required by USD.

#### Linux (Ubuntu 20.04)

Clang 11 can be installed with the following commands:
```
sudo apt-get update
sudo apt-get install -y build-essential clang-11 clang-format-11 clang-tidy-11
```

The default compiler is gcc, so you will need to configure clang-11 as the default compiler using the `update-alternatives` command.

Certain libraries must be installed before building on Linux. These can be installed with the following command:

```
sudo apt-get -y install zlib1g-dev libjpeg-turbo8-dev libtiff-dev libpng-dev libglm-dev libglew-dev libglfw3-dev libgtest-dev libgmock-dev
```

Note that on Windows, these same libraries are built as part of the build instructions below.

### GPU

Aurora requires a GPU with hardware ray tracing support, either through **DirectX Raytracing** (DXR) on Windows, or **Vulkan Ray Tracing** on Windows or Linux. These include, but are not limited to:

- **NVIDIA GPUs with native ray tracing support** include any GPU with "RTX" in the brand name, including mobile GPUs. This includes:
  - The GeForce RTX series, such as the GeForce RTX 2060.
  - The Quadro RTX series, such as the Quadro RTX 4000.
  - The RTX A series, such as the RTX A2000.
- **NVIDIA GPUs with compute-based support** include any GPU with the "Pascal" microarchitecture and at least 6 GB of RAM. Note that these GPUs will perform substantially slower with GPU ray tracing due to the lack of native ray tracing support. This includes:
  - The GeForce 10 series, such as the GeForce GTX 1080.
  - The Quadro P series, such as the Quadro P4000.
- **AMD GPUs with native ray tracing support** include any GPU with the "RDNA 2" microarchitecture. This includes:
  - The RX 6000 and RX 7000 series, including the mobile RX 6000M series.
  - The Radeon PRO W6000 series, such as the Radeon PRO W6800.
  - The Ryzen 7 6000 series of mobile processors, which have 600M series integrated GPUs.
- **Intel GPUs with native ray tracing support** include any GPU with “Xe” architecture and DX12 support. This includes:
  - The Intel™ Arc® Pro A-series for workstations, such as the Intel Arc Pro A40 and Intel Arc Pro A50.
  - The Intel™ Arc® A-series, such as the Intel Arc A380 and Intel Arc A770.

## Graphics API Support

As noted in the system requirements above, Aurora can use either the [DirectX Raytracing](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html) API (Windows only) or the [Vulkan Ray Tracing](https://www.khronos.org/blog/ray-tracing-in-vulkan) API (on Windows or Linux). These are referred to as "backends" in the build process.

On Windows, you can set a flag in the CMake configuration to enable the desired backend(s):
- `-D ENABLE_DIRECTX_BACKEND=[ON/OFF]` for DirectX (default is ON).
- `-D ENABLE_HGI_BACKEND=[ON/OFF]` for Vulkan (default is OFF).

On Linux,  `ENABLE_HGI_BACKEND` is `ON` and `ENABLE_DIRECTX_BACKEND` is `OFF` and cannot be changed.

Vulkan support is provided through USD Hydra's "HGI" interface, using a prototype extension for Vulkan ray tracing available in [this branch of the Autodesk fork of USD](https://github.com/autodesk-forks/USD/tree/adsk/feature/hgiraytracing). For this reason, USD is required when compiling Aurora with the Vulkan backend on Windows or Linux. USD is built as part of the build process described below, to support both the HdAurora render delegate and Vulkan.

NOTE: At this time Vulkan is supported on NVIDIA GPUs only.

## Quick Start

Follow these steps to build Aurora and its dependencies and run the sample application.

#### Windows

Run the following on a command prompt with compiler tools, such as "x64 Native Tools Command Prompt for VS 2019".

```
python Scripts\installExternals.py ..\AuroraExternals
cmake -S . -B Build -D EXTERNALS_DIR=..\AuroraExternals
cmake --build Build --config Release
cd Build\bin\Release
Plasma.exe
```
#### Linux (Ubuntu 20.04)
```
python Scripts/installExternals.py ../AuroraExternals
cmake -S . -B Build -D EXTERNALS_DIR=../AuroraExternals/Release
cmake --build Build
cd Build/bin/Release
./Plasma --output {OUTPUT_IMAGE_FILE.png} --scene {INPUT_SCENE_FILE.obj} --renderer hgi
```
## Building Aurora

Aurora includes a script that retrieves and builds dependencies ("externals") from source. This script is based on the [USD build script](https://github.com/PixarAnimationStudios/USD/tree/release/build_scripts). CMake is used to build Aurora directly, or to create an IDE project which can then be used to build or debug Aurora.

1. **Download or clone** the contents of this repository to a location of your choice. Cloning with Git is not strictly necessary as Aurora does not currently use submodules. We refer to this location as "AURORA_DIR".

2. **Start a command line** with access to your C++ compiler tools. When using Visual Studio, the "x64 Native Tools Command Prompt for VS 2019" shortcut will provide the proper environment. The CMake and Python executables must also be available, through the PATH environment variable.

3. **Installing externals:** Run *[Scripts/installExternals.py](Scripts/installExternals.py)* with Python in AURORA_DIR to build and install externals.
   
   - You can run the install script with the desired location for storing and compiling externals as the only argument. We will refer to this location as "EXTERNALS_DIR".
     ```
     python Scripts/installExternals.py {EXTERNALS_DIR}
     ```
     
   - The script will retrieve the source code for all dependencies using available release packages, or clone with Git as needed. The script will also compile all of the dependencies. If you want more feedback on the script execution, you can run the script with the `-v` option for verbose output.
   
   - The entire process takes about 30 minutes to run, and consumes about 10 GB of disk space in EXTERNALS_DIR. While USD is being compiled, the script will use most of the CPU cores, and your computer may be sluggish for a few minutes.
   
   - Use the `--build-variant` option to choose the build configuration of externals. It can be `Debug`, `Release` (default), or `All`. On Windows, this option needs to match the CONFIGURATION value used in the next step.
   
   - Use the `-h` option with the script to see available options.
   
4. **Generating projects:** Run CMake in AURORA_DIR to generate build projects, e.g. a Visual Studio solution.

   - You must specify the externals installation directory (EXTERNALS_DIR, above) as a CMake path variable called `EXTERNALS_DIR`. If you are using cmake-gui, you should specify this variable before generating.

   - You must specify a build directory, and we refer to this location as "AURORA_BUILD_DIR". The recommended build directory is {AURORA_DIR}/Build, which is filtered by [.gitignore](.gitignore) so it won't appear as local changes for Git.

   - You can use CMake on the command line or the GUI (cmake-gui). The CMake command to generate projects is as follows:

     ```
     cmake -S . -B {AURORA_BUILD_DIR} -D CMAKE_BUILD_TYPE={CONFIGURATION} -D EXTERNALS_DIR={EXTERNALS_DIR}
     ```

   - The CONFIGURATION value can be one of `Debug` or `Release` (default).

   - You can optionally specify the desired graphics API backend as described above, e.g. `-D ENABLE_HGI_BACKEND=ON`.

   - On Windows, you may need to specify the toolchain and architecture with `-G "Visual Studio 16 2019" -A x64`.

5. **Building:** You can load the *Aurora.sln* Visual Studio solution file from the Aurora build directory, and build Aurora using the build configuration used with the *installExternals.py* script (see below), or use CMake.

   - The CMake command to build Aurora is as follows:

     ```
     cmake --build {AURORA_BUILD_DIR} --config {CONFIGURATION} -v
     ```

   - The build for a single build configuration (e.g. Debug or Release) takes about a minute and consumes about 500 MB of disk space.


### Changing Configurations

By default, Aurora will be built with the *Release* build configuration, i.e. for application deployment and best performance. To change to another configuration, see the information below.

- **For the externals** installed with the *installExternals.py* script, you can specify the desired configuration with the `--build-variant` option, and specify `Debug`, `Release` (default), or `All`. Note that you can have multiple configurations built.
- **For Aurora itself**, you can specify the desired configuration with the `CMAKE_BUILD_TYPE` variable when running CMake project generation, with the same values as above.
- On Windows, it is necessary for Aurora *and* the externals be built with the same configuration. Since Visual Studio allows multiple configurations in a project, you must select the appropriate configuration in the Visual Studio interface, or you will get linker errors.
- On Linux, due to a CMake error in the debug configuration of OpenImageIO, `--build-variant` is disabled and only the release configuration of externals is installed. However, you can build either the debug or release configuration of Aurora with the release configuration of externals. To do this, specify the configuration *subdirectory* with the `EXTERNALS_DIR` variable, e.g. {EXTERNALS_DIR}/Release.

## Running Aurora

Aurora can be exercised in three ways:

- Using the [Plasma](Doc/Plasma.md) sample application, either interactively or on the command line.
- Using the [HdAurora](Doc/HdAurora.md) render delegate, through a compliant USD Hydra-based application like [Usdview](https://graphics.pixar.com/usd/release/toolset.html) or certain design applications.
- Using the Aurora unit tests, which use the [Google Test](https://github.com/google/googletest) framework: [Foundation](Tests/Foundation), [AuroraInternals](Tests/AuroraInternals), and [Aurora](Tests/Aurora).

All of these are built with Aurora, and binaries can be found in the build output directory after following the instructions above.

## Sample Data

The Autodesk Telescope model shown above was developed by [Roberto Ziche](https://robertoziche.com), and was inspired by Celestron products. It is made available for use with Aurora or any another application under [the CC BY 4.0 license](https://creativecommons.org/licenses/by/4.0). You can [download a package](https://drive.google.com/file/d/1RM09qDOGcRinLJTbXCsiRfQrHmKA-1aN/view?usp=share_link) containing an OBJ version for use with Plasma, or a USD version for use with Usdview or other applications.

Other recommended sources of data include the [McGuire Computer Graphics Archive](https://casual-effects.com/data) and the [ASWF USD Working Group](https://wiki.aswf.io/display/WGUSD/Sample+Assets).

## Documentation

Available documentation can by found in the [Doc](Doc) directory. This includes the following:

- [Plasma](Doc/Plasma.md): instructions for using the sample application.
- [HdAurora](Doc/HdAurora.md): instructions for using the sample application.
- [Coding standards](Doc/CodingStandards.md).

More information about contributions and licensing can be found here:
- [How to contribute to the project](CONTRIBUTING.md).
- [The software license](LICENSE.md), which is the Apache License 2.0.
