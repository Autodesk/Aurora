# Plasma

**Plasma** is a small reference application used to exercise Aurora, without requiring the use of a full Autodesk application or USD Hydra scene delegate. Plasma can load OBJ files like the one shown below, along with certain material properties. See the usage instructions below. The [McGuire Computer Graphics Archive](https://casual-effects.com/data) is a good resource for OBJ files.

<img src="Plasma.jpg" style="zoom: 50%;" />

## System Requirements

Plasma has the same [system requirements](../README.md) as Aurora. In particular, a GPU with support for DirectX Raytracing or Vulkan Ray Tracing is required.

At this time, interactive use of Plasma is only supported on Windows. For Linux, command-line single image rendering is available as described below, and interactive support is coming soon.

## Using Plasma

| Feature                                             | Input                                                        |
| :-------------------------------------------------- | :----------------------------------------------------------- |
| Load an OBJ File                                    | CTRL-O key *OR*<br />Drag and drop *OR*<br />File path as first command line argument |
| Load an environment image (HDR format)              | CTRL-E key *OR*<br />Drag and drop                           |
| Load a MaterialX document (.mltx)                   | CTRL-M key: Applies to all loaded objects<br />M: Reload the previous document<br />R key: Reset materials to original |
| Save screenshot as PNG (to last accessed directory) | S key                                                        |
| Set to Full Screen                                  | F11 key                                                      |
| Toggle v-sync                                       | V key                                                        |
| Toggle animation                                    | Space key                                                    |
| Toggle tone mapping                                 | T key                                                        |
| Toggle directional light                            | L key                                                        |
| Toggle diffuse only rendering                       | D key                                                        |
| Toggle denoising                                    | SHIFT-D key                                                  |
| Toggle transparent shadows *                        | O key                                                        |
| Toggle ground plane                                 | G: matte shadow<br />SHIFT-G: matte reflection               |
| Cycle importance sampling                           | I (letter) key, cycles in the following order: <br />1) Multiple importance sampling (MIS)<br />2) BSDF importance sampling<br />3) Environment light importance sampling |
| Adjust exposure (½ stop increments)                 | + key (increase) and - key (decrease)                        |
| Adjust max luminance for samples (firefly clamping) | CTRL+ key (increase) and CTRL- key (decrease)<br />(full stops starting at 1000 luminance) |
| Adjust trace depth (path length) **                 | [ key (increase) and ] key (decrease)<br />(default 5, range is [1, 10]) |
| Display debug buffers (AOVs)                        | ~ = Output (beauty)<br />1 = Output with errors<br />2 = ViewZ (depth)<br />3 = Normals<br />4 = Base Color<br />5 = Roughness<br />6 = Metalness <br /><br />The following are used for denoising:<br />7 = Diffuse radiance<br />8 = Diffuse hit distance<br />9 = Glossy radiance<br />0 = Glossy hit distance |
| Orbit View                                          | Left click and drag                                          |
| Pan View                                            | Right click and drag                                         |
| Dolly View                                          | Middle click and drag *OR*<br />Mouse wheel                  |
| Fit View to Scene                                   | F key                                                        |
| Quit                                                | ESC key                                                      |

\* This is a performance optimization.

** It may be necessary to increase trace depth for overlapping transparent / transmissive surfaces, at the cost of performance.

## Command Line Rendering
The following command renders an output image with the specified scene file. The optional `--renderer hgi` argument enables the HGI backend in the renderer, with DirectX used as the default backend. On Linux, you must use the HGI backend.
```
Plasma --output {OUTPUT_FILE_NAME.png} --scene {INPUT_SCENE.obj} [--renderer hgi]
```

## OBJ Material Properties in Plasma

The following properties from OBJ MTL files are supported. OBJ MTL properties not listed here are ignored. Materials are converted to use the [Autodesk Standard Surface](https://autodesk.github.io/standard-surface) material model.

| Standard Surface   | OBJ MTL File | Notes                                                        |
| :----------------- | :----------- | :----------------------------------------------------------- |
| base_color         | Kd / map_Kd  | Image supported.                                             |
| metalness          | Pm           |                                                              |
| specular_color     | Ks           |                                                              |
| specular_roughness | Pr / map_Pr  | Image supported.<br /><br />"Ns" for non-physical shininess is not supported; use Pr instead. |
| specular_IOR       | Ni           | This is the index of refraction, e.g. 1.5 is a good value for glass. |
| transmission       | Tr / d       | The two values are inverses of each of other, i.e. *d* of 1 is the same as *Tr* of 0, i.e. opaque. It may be necessary to increase the trace depth (described above) for certain models to look correct.<br /><br /> *Tr / d* in OBJ are typically used for opacity (transparency) but here they are used for *transmission* instead, which is more interesting, e.g. for refraction. If values for both are present, the value for *d* will be used. |
| transmission_color | Tf           | Black (the default) is assumed to be useless and will be interpreted as white (no tint). |
| opacity            | map_d        | Image only.<br /><br />This is different from *transmission*. In Standard Surface, opacity refers to *transparency*, which is simple alpha blending. It was determined that it is more useful for *map_d* to be used for opacity, to support cutouts. |
| normal             | bump / norm  | Image only, and it must be a normal map (not a height map)   |