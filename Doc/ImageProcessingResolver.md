# Image Processing Resolver

The **ImageProcessingResolver** is a USD `ArResolver` plugin that allows image files to be pre-processed via an asset URI system prior to loading in Aurora and other parts of USD.

If asset URI is added to a USDA file like this, in place of a filename, it will be parsed by the resolver to perform a environment map layout conversion on the file `c:/testart/HDRI/StandardCubeMap.hdr`.  The results of the image processing operation will be passed to USD as if the processing has been carried out on an external file.

```
asset inputs:file = @imageProcessing://convertEnvMapLayout?file=C%3A%2Ftestart%2Fhdri%2FStandardCubeMap.hdr&sourceLayout=cube&targetLayout=latlon@
```

## Asset URIs

The image processing URI is an encoded description of the image processing operation required, encoded as a W3C [Universal Resource Identifier](https://www.w3.org/wiki/URI), e.g:

```
imageProcessing://linearize/?filename=Background.exr&exposure=2.5
```

The URI components are interpreted as follows to perform the image processing operation:

`Scheme`: The scheme (the part before colon) must be imageProcessing 

`Host`: The host (the part between the first //  and the first / ) describes the image processing operation.  See below for supported operations.

`Query`: The query parameters (the first preceded by a ? the remaining parameters preceeded by a &)  describes the parameters to the operations.  The `file` query parameter is common to all operations the filename of the image file to be processed. Note this must be encoded so any invalid chars are represented as character codes(e.g. G%C3%BCnter).  The filename itself could be an image processing URI to allow multiple image processing operations.
 
## Supported operations

### Linearize
If the host name in the URI is `linearize`, the image is linearized, that is an "inverse tonemapping" operation is carried out so the result after Canon tonemapping, with the provided exposure is the same in the original image file.

Parameters:
* file: File to be linearized (must be RGB 32-bit floating point .EXR or .HDR image)
* exposure: For linearization the exposure used in the tone mapping operation to be reversed.

### Convert environment map layout
If the host name in the URI is `convertEnvMapLayout`, the image is treated as an environment map and converted from one layout to another. Currently only conversion from a "cross" cube map layout to lat-long equirectangular environment map is supported.

Parameters
* file: File to be converted (must be RGB 32-bit floating point .EXR or .HDR image)
* sourceLayout: Environment map layout to be converted from (currently must be `cube`)
* targetLayout: Environment map layout to be converted to (currently must be `latlon`)
