# HdAurora

**HdAurora** is a USD Hydra render delegate that allows Aurora to be used in applications that implement a USD Hydra scene delegate.

Hydra is a framework used by many applications such as [Autodesk Maya](https://www.autodesk.com/products/maya) to support multiple renderers implemented as render delegates. Examples of other render delegates include [HdStorm](https://graphics.pixar.com/usd/dev/api/hd_storm_page_front.html), a rasterization renderer that uses OpenGL, Vulkan, and Metal, and [HdArnold](https://github.com/Autodesk/arnold-usd), which enables the use of [Autodesk Arnold](https://arnoldrenderer.com).

To learn more about Hydra, see [this presentation](https://graphics.pixar.com/usd/files/Siggraph2019_Hydra.pdf) from SIGGRAPH 2019 as well as [the USD documentation](https://graphics.pixar.com/usd/release/intro.html).

## usdview

The easiest way to run HdAurora is using the **usdview** tool, which is part of the USD toolset that is used for viewing USD files.

<img src="USDView.JPG" style="zoom: 50%;" />

Once **HdAurora** has been deployed to your USD folder you can select HdAurora in **usdview** using the renderer menu with the **Aurora** option:
<img src="RendererMenu.jpg" width="50%;" />

## Deploying HdAurora
In this section, we explain how to run usdview using HdAurora with the default Aurora build, and how to deploy HdAurora to a custom USD installation if it is preferred. The commands in this section require "x64 Native Tools Command Prompt for VS 2019".

### Running usdview with Default Aurora Build
If Aurora is built with the default build steps, **usdview** and required USD binaries are automatically deployed with HdAurora binaries. Assuming the root of Aurora source tree is *AURORA_ROOT*, the following build steps deploy **usdview** automatically:
```
cd %AURORA_ROOT%
python Scripts\installExternals.py --build-variant Release ..\AuroraExternals
cmake -S . -B Build
cmake --build Build --config Release
```
Before running **usdview** with **HdAurora**, you need to configure the run environment with:
```
set PYTHONPATH=%AURORA_ROOT%\Build\bin\Release\python
set PATH=%PATH%;%AURORA_ROOT%\Build\bin\Release
```
To render the [Autodesk Telescope USD model](https://drive.google.com/file/d/1RM09qDOGcRinLJTbXCsiRfQrHmKA-1aN/view?usp=share_link) with **HdAurora**, simply extract the ZIP file into *ASSET_DIR*, and run the following command.
```
usdview %ASSET_DIR%\AutodeskTelescope.usda --renderer=Aurora
```

### Deploying HdAurora to Custom USD Installation
For users who want to use their own USD build, Aurora provides [deployHdAurora.py](../Scripts/deployHdAurora.py) to deploy **HdAurora** to the custom USD installation. [deployHdAurora.py](../Scripts/deployHdAurora.py) can also create a new USD installation if one does not exist, using the `--build` option.

Run the following command to build a new USD installation located at *..\USD* relative to the the root of Aurora source tree ( *AURORA_ROOT*). This will take about 30 minutes to complete and will require 10 GB of disk space.
```
cd %AURORA_ROOT%
python Scripts\deployHdAurora.py ..\USD --externals_folder=..\AuroraExternals --config=Release --build
```

You can then run **usdview** from the *bin* subdirectory within that installed USD directory. To render the [Autodesk Telescope USD model](https://drive.google.com/file/d/1RM09qDOGcRinLJTbXCsiRfQrHmKA-1aN/view?usp=share_link) with **HdAurora**, simply extract the ZIP file into *ASSET_DIR* and run the following commands.

```
cd ..\USD\bin
usdview %ASSET_DIR%\AutodeskTelescope.usda --renderer=Aurora
```

