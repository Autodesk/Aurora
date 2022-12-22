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

The provided Python script [deployHdAurora.py](../Scripts/deployHdAurora.py) can be used to deploy **HdAurora** to a USD installation. This can also create a new USD installation if one does not exist, using the `--build` option.

Run the following command to create a new USD installation located at *..\USD* relative to the Aurora repository root and deploy **HdAurora** to it. Creating a new USD installation requires a command prompt with compiler tools, such as "x64 Native Tools Command Prompt for VS 2019", and should be run from the root of the Aurora repository. This will take about 30 minutes to complete and will require 10 GB of disk space.

```
python Scripts\deployHdAurora.py ..\USD --externals_folder=..\AuroraExternals --config=Release --build
```

You can then run **usdview** from the *bin* subdirectory within that installed USD directory. The example command below loads the [Autodesk Telescope USD model](https://drive.google.com/file/d/1RM09qDOGcRinLJTbXCsiRfQrHmKA-1aN/view?usp=share_link); simply extract the ZIP file into a directory, referred to as *ASSET_DIR* below.

```
cd ..\USD\bin
python usdview {ASSET_DIR}\AutodeskTelescope.usda --renderer=Aurora
```

