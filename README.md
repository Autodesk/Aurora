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
- A USD Hydra render delegate ([HdAurora](Doc/HdAurora.md)) and standalone sample application ([Plasma](Doc/Plasma.md)).

... with new features and enhancements to performance and quality planned. This will include denoising with [NVIDIA Real-Time Denoisers](https://developer.nvidia.com/rtx/ray-tracing/rt-denoisers), support for alternative material models, discrete light sources, and more.

## System Requirements

### Operating System

Aurora is officially supported on **Windows 10** or **Ubuntu 20.04**. Windows 11 and other Linux distributions may work, but are not yet supported.

To run Aurora, the latest GPU drivers from [NVIDIA](https://www.nvidia.com/download/index.aspx), [AMD](https://www.amd.com/en/support), or [Intel](https://www.intel.com/content/www/us/en/download-center/home.html) are recommended as ray tracing API support is being actively improved. No other software is required to run Aurora.

### Build Software

Software required for building Aurora can be found in [the build instructions](Doc/Build.md).

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

See [the build instructions](Doc/Build.md) for information on enabling support for DirectX Raytracing or Vulkan Ray Tracing.

NOTE: At this time Vulkan is supported on NVIDIA GPUs only.

## Quick Start

Follow these steps to build Aurora and its dependencies and run the sample application, Plasma.

More details on building Aurora can be found [here](Doc/Build.md), including options and possible issues you may encounter.

#### Windows

Run the following on a command prompt with compiler tools, such as "x64 Native Tools Command Prompt for VS 2019".

```
python Scripts\installExternals.py ..\AuroraExternals
cmake -S . -B Build
cmake --build Build --config Release
cd Build\bin\Release
Plasma.exe
```
#### Linux (Ubuntu 20.04)
```
python Scripts/installExternals.py ../AuroraExternals
cmake -S . -B Build
cmake --build Build
cd Build/bin/Release
./Plasma --output {OUTPUT_IMAGE_FILE.png} --scene {INPUT_SCENE_FILE.obj} --renderer hgi
```
## Running Aurora

Aurora can be exercised in three ways:

- Using the [Plasma](Doc/Plasma.md) sample application, either interactively or on the command line.
- Using the [HdAurora](Doc/HdAurora.md) render delegate, through a compliant USD Hydra-based application like [Usdview](https://graphics.pixar.com/usd/release/toolset.html) or certain design applications.
- Using the Aurora unit tests, which use the [Google Test](https://github.com/google/googletest) framework: [Foundation](Tests/Foundation), [AuroraInternals](Tests/AuroraInternals), and [Aurora](Tests/Aurora).

All of these are built with Aurora, and binaries can be found in the build output directory after following the build instructions.

## Sample Data

The Autodesk Telescope model shown above was developed by [Roberto Ziche](https://robertoziche.com), and was inspired by Celestron products. It is made available for use with Aurora or any another application under [the CC BY 4.0 license](https://creativecommons.org/licenses/by/4.0). You can [download a package](https://drive.google.com/file/d/1RM09qDOGcRinLJTbXCsiRfQrHmKA-1aN/view?usp=share_link) containing an OBJ version for use with Plasma, or a USD version for use with Usdview or other applications.

Other recommended sources of data include the [McGuire Computer Graphics Archive](https://casual-effects.com/data) and the [ASWF USD Working Group](https://wiki.aswf.io/display/WGUSD/Sample+Assets).

## Documentation

Available documentation can by found in the [Doc](Doc) directory. This includes the following:

- [Building](Doc/Build.md): instructions for building Aurora, including options.
- [Plasma](Doc/Plasma.md): instructions for using the sample application.
- [HdAurora](Doc/HdAurora.md): instructions for using the Hydra render delegate.
- [Coding standards](Doc/CodingStandards.md).

More information about contributions and licensing can be found here:
- [How to contribute to the project](CONTRIBUTING.md).
- [The software license](LICENSE.md), which is the Apache License 2.0.
