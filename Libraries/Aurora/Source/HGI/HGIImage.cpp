// Copyright 2022 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "pch.h"

#include "HGIImage.h"
#include "HGIRenderer.h"

using namespace pxr;

BEGIN_AURORA

HGIImage::HGIImage(HGIRenderer* pRenderer, const IImage::InitData& initData)
{
    // Convert Aurora image format to HGI image format.
    uint32_t pixelSizeBytes;
    HgiFormat hgiFormat = getHGIFormat(initData.format, initData.linearize, &pixelSizeBytes);

    // Create descriptor for image.
    HgiTextureDesc imageTexDesc;
    imageTexDesc.debugName      = initData.name;
    imageTexDesc.format         = hgiFormat;
    imageTexDesc.dimensions     = GfVec3i(initData.width, initData.height, 1);
    imageTexDesc.layerCount     = 1;
    imageTexDesc.mipLevels      = 1;
    imageTexDesc.usage          = HgiTextureUsageBitsShaderRead;
    imageTexDesc.pixelsByteSize = initData.width * initData.height * pixelSizeBytes;
    imageTexDesc.initialData    = initData.pImageData;

    // Create the texture.
    _texture = HgiTextureHandleWrapper::create(
        pRenderer->hgi()->CreateTexture(imageTexDesc), pRenderer->hgi());
}

HgiFormat HGIImage::getHGIFormat(ImageFormat format, bool linearize, uint32_t* pPixelByteSizeOut)
{
    // Convert the HGI format to HGI format enum.
    switch (format)
    {
    case ImageFormat::Integer_RGBA:
        *pPixelByteSizeOut = 4;
        return linearize ? HgiFormat::HgiFormatUNorm8Vec4srgb : HgiFormat::HgiFormatUNorm8Vec4;
    case ImageFormat::Float_RGBA:
        *pPixelByteSizeOut = 16;
        return HgiFormat::HgiFormatFloat32Vec4;
    case ImageFormat::Float_RGB:
        *pPixelByteSizeOut = 12;
        return HgiFormat::HgiFormatFloat32Vec3;
    default:
        break;
    }

    AU_FAIL("Unsupported image format:%x", format);
    return (HgiFormat)-1;
}

END_AURORA
