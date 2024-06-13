# Building

## Prerequisites

Several prerequisites must be installed before building Aurora.

### Windows
On windows the following packages should be installed and added to the system PATH environment variable:
* [Microsoft Visual Studio 2019](https://my.visualstudio.com/Downloads?q=visual%20studio%202019) or [2022](https://my.visualstudio.com/Downloads?q=visual%20studio%202022).
* CMake 3.29.3 or later ([installer](https://github.com/Kitware/CMake/releases/download/v3.29.7/cmake-3.29.7-windows-x86_64.msi)).
* Python 3.11 or later ([installer](https://www.python.org/downloads/release/python-3119/)) and the following Python packages:
  * PySide6: install with `pip3 install pyside6`.
  * PyOpenGL: install with `pip3 install PyOpenGL`.
  * Jinja2: install with `pip3 install Jinja2`.

* NASM 2.16 ([installer](https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/win64/nasm-2.16.01-installer-x64.exe)).
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows). This should be accessible via the `VULKAN_SDK` environment variable.

### MacOS
The following tools should be installed and accessible through PATH environment variable:
* Xcode 15.4 or later.
  * Xcode Command Line Tools: install by running `xcode-select --install`.
* Git with LFS: set up by running `git lfs install`.
* CMake 3.29.3 or later ([installer](https://github.com/Kitware/CMake/releases/download/v3.29.7/cmake-3.29.7-macos-universal.dmg)).
* Python 3.11 or later.

### Linux
On Ubuntu 22.04, the dependencies can be installed with the following Advanced Package Tool command:
```
sudo apt-get -y install zlib1g-dev libjpeg-turbo8-dev libtiff-dev libpng-dev libglm-dev libglew-dev libglfw3-dev libgtest-dev libgmock-dev libxt-dev
```

Tools including git, g++/clang, CMake, Python, [VulkanSDK](https://vulkan.lunarg.com/) are also requested.

## Building Aurora

Aurora includes a script that retrieves and builds dependencies ("externals") from source. This script is based on the [USD build script](https://github.com/PixarAnimationStudios/USD/tree/release/build_scripts). CMake is used to build Aurora directly, or to create an IDE project which can then be used to build and debug Aurora.

1. **Download or clone** the contents of this repository to a location of your choice. Cloning with Git is not strictly necessary as Aurora does not currently use submodules. We refer to this location as *AURORA_DIR*.

2. **Start a command line** with access to your C++ compiler tools. When using Visual Studio, the "x64 Native Tools Command Prompt for VS 2019" (or 2022) shortcut will provide the proper environment. The CMake and Python executables must also be available, through the PATH environment variable.

3. **Installing externals:** Run *[Scripts/installExternals.py](/Scripts/installExternals.py)* with Python in *AURORA_DIR* to build and install externals.

   - You can run the install script with the desired location for storing and compiling externals as the only argument. We will refer to this location as *EXTERNALS_ROOT*.
     ```
     python Scripts/installExternals.py {EXTERNALS_ROOT}
     ```

   - The script will retrieve the source code for all dependencies using available release packages, or clone with Git as needed. The script will also compile all of the dependencies. If you want more feedback on the script execution, you can run the script with the `-v` option for verbose output.

   - The entire process takes about 30 minutes to run, and consumes about 10 GB of disk space in *EXTERNALS_ROOT*. While USD is being compiled, the script will use most of the CPU cores, and your computer may be sluggish for a few minutes.

   - Use the `--build-variant` option to choose the build configuration of externals. It can be `Debug`, `Release` (default), or `All`.

   - On MacOS, you can specify the build target with `--build-target`. It can be `native`, `x86_64`, `arm64`, or `universal`.

   - Use the `-h` option with the script to see available options.

4. **Generating projects:** Run CMake in *AURORA_DIR* to generate build projects, e.g. a Visual Studio solution.

   - You may specify the externals installation directory (*EXTERNALS_ROOT*, above) as a CMake path variable called `EXTERNALS_ROOT`. This must be specified as an absolute path, e.g. with a drive letter on Windows. If no `EXTERNALS_ROOT` is specified, the `EXTERNALS_ROOT` built by the latest run of *installExternals.py* will be used automatically. If you are using cmake-gui, you should specify this variable before generating.

   - You must specify a build directory, and we refer to this location as *AURORA_BUILD_DIR*. The recommended build directory is *{AURORA_DIR}/Build*, which is filtered by [.gitignore](/.gitignore) so it won't appear as local changes for Git.

   - You can use CMake on the command line or the GUI (cmake-gui). The CMake command to generate projects is as follows:

     ```
     cmake -S . -B {AURORA_BUILD_DIR} -G {GENERATOR} -D CMAKE_BUILD_TYPE={CONFIGURATION} -D EXTERNALS_ROOT={EXTERNALS_ROOT}
     ```

     As noted above, the value for `EXTERNALS_ROOT` must be specified as an absolute path.

   - The *CONFIGURATION* value can be one of `Debug` or `Release` (default).

   - On Windows, `CMAKE_BUILD_TYPE` is ignored during the cmake configuration. You are required to specify the build configuration with `--config {CONFIGURATION}` during the cmake build.

   - You can optionally specify the desired graphics API backend as described below, e.g. `-D ENABLE_HGI_BACKEND=ON`. Note that Plasma with Vulkan backend does not support interactive mode yet. If more than one backend is enabled, you can designate the desired one, e.g. `--renderer hgi`, when executing Plasma.

   - On Windows, you may need to specify the toolchain and architecture with `-G "Visual Studio 16 2019" -A x64` or `-G "Visual Studio 17 2022" -A x64`.

   - On MacOS, you can specify the toolchain with `-G "Xcode"`. This will also help to generate environment variables, etc. needed by Plasma.

5. **Building:** You can load the *Aurora.sln* Visual Studio solution file (Windows) / *Aurora.xcodeproj* Xcode project file (MacOS) from the Aurora build directory, and build Aurora using the build configuration used with the *installExternals.py* script (see below), or use CMake.

   - The CMake command to build Aurora is as follows:

     ```
     cmake --build {AURORA_BUILD_DIR} --config {CONFIGURATION} -v
     ```
   - `--config {CONFIGURATION}` is only required on Windows.
   - The build for a single build configuration (e.g. Debug or Release) takes about a minute and consumes about 500 MB of disk space.

## Graphics API Support

As noted in the system requirements, Aurora can use either the [DirectX Raytracing](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html) API (Windows only) or the [Vulkan Ray Tracing](https://www.khronos.org/blog/ray-tracing-in-vulkan) API (on Windows or Linux). These are referred to as "backends" in the build process.

On Windows, you can set a flag in the CMake configuration to enable the desired backend(s):

- `-D ENABLE_DIRECTX_BACKEND=[ON/OFF]` for DirectX (default is ON).
- `-D ENABLE_HGI_BACKEND=[ON/OFF]` for Vulkan (default is OFF).

On Linux,  `ENABLE_HGI_BACKEND` is `ON` and `ENABLE_DIRECTX_BACKEND` is `OFF` and cannot be changed.

Vulkan support is provided through USD Hydra's "HGI" interface, using a prototype extension for Vulkan ray tracing available in [this branch of the Autodesk fork of USD](https://github.com/autodesk-forks/USD/tree/adsk/feature/hgiraytracing). For this reason, USD is required when compiling Aurora with the Vulkan backend on Windows or Linux. USD is built as part of the build process described above, to support both the HdAurora render delegate and Vulkan.


## Changing Configurations

By default, Aurora will be built with the *Release* build configuration, i.e. for application deployment and best performance. To change to another configuration, see the information below.

- **For the externals** installed with the *installExternals.py* script, you can specify the desired configuration with the `--build-variant` option, and specify `Debug`, `Release` (default), or `All`. You can have both `Debug` and `Release` configurations built with `All` option. Additional configurations will take longer to install and will consume more disk space.
- **For Aurora itself**, on Linux, you can specify the desired configuration with the `CMAKE_BUILD_TYPE` variable when running CMake project generation. On Windows, this variable is ignored.
- On Windows, it is necessary for Aurora *and* the externals be built with the same configuration. Since Visual Studio allows multiple configurations in a project, you must select the appropriate configuration in the Visual Studio interface, or you will get linker errors. `--build-variant All` is recommended to support both `Debug` and `Release` build configurations.
