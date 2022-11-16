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
#pragma once

#include "ImageBase.h"

BEGIN_AURORA

// Forward declarations.
class PTRenderer;

// An internal implementation for IImage.
class PTImage : public ImageBase
{
public:
    /*** Lifetime Management ***/

    PTImage(PTRenderer* pRenderer, const IImage::InitData& initData);
    ~PTImage() {};

    /*** Static Functions ***/

    // Gets the number of bytes per pixel for the specified IImage format.
    static size_t getBytesPerPixel(ImageFormat format);

    // Gets the corresponding DXGI format for the specified IImage format, optionally with sRGB
    // linearization.
    static DXGI_FORMAT getDXFormat(ImageFormat format, bool linearize = false);

    // Creates an SRV for the specified image (which may be null) at the specified handle in a
    // descriptor heap.
    static void createSRV(
        const PTRenderer& renderer, PTImage* pImage, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

    /*** Functions ***/

    // Gets the image texture.
    ID3D12Resource* texture() { return _pTexture.Get(); }

    // Gets the image alias map, if this image represents an environment.
    ID3D12Resource* aliasMap() { return _pAliasMapBuffer.Get(); }

    // Gets the integral of the luminance over the entire environment, if this image represents an
    // environment.
    float luminanceIntegral() { return _luminanceIntegral; }

private:
    /*** Private Functions ***/

    void initResource(const void* pImageData);
    void initAliasMap(const void* pImageData);

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    ID3D12ResourcePtr _pTexture;
    ID3D12ResourcePtr _pAliasMapBuffer;
    ImageFormat _format = ImageFormat::Integer_RGBA;
    bool _linearize     = true;
    uvec2 _dimensions;
    string _name = "UNINITIALIZED";
};
MAKE_AURORA_PTR(PTImage);

END_AURORA