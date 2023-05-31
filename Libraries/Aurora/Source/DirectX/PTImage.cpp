// Copyright 2023 Autodesk, Inc.
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

#include "AliasMap.h"
#include "PTImage.h"

#include "PTRenderer.h"

BEGIN_AURORA

PTImage::PTImage(PTRenderer* pRenderer, const IImage::InitData& initData)
{
    // Image data must be provided, along with valid dimensions.
    assert(initData.pImageData && initData.width > 0 && initData.height > 0);

    // Copy the basic properties.
    _pRenderer  = pRenderer;
    _format     = initData.format;
    _linearize  = initData.linearize;
    _dimensions = uvec2(initData.width, initData.height);
    _name       = initData.name;

    // Prepare the D3D12 texture resource.
    initResource(initData.pImageData);

    // If the image is to be used as an environment image, prepare an alias map for importance
    // sampling of that environment image.
    if (initData.isEnvironment)
    {
        // Create a transfer buffer for the alias map data.
        size_t bufferSize             = _dimensions.x * _dimensions.y * sizeof(AliasMap::Entry);
        TransferBuffer transferBuffer = _pRenderer->createTransferBuffer(bufferSize);

        // Build the alias map directly in the mapped buffer.
        AliasMap::Entry* pMappedData = reinterpret_cast<AliasMap::Entry*>(transferBuffer.map());
        AliasMap::build(static_cast<const float*>(initData.pImageData), _dimensions, pMappedData,
            bufferSize, _luminanceIntegral);
        transferBuffer.unmap();

        // Retain the GPU buffer pointer from the transfer buffer (upload buffer will be deleted
        // after upload complete.)
        _pAliasMapBuffer = transferBuffer.pGPUBuffer;
    }
}

// Gets the number of bytes per pixel for the specified IImage format.
size_t PTImage::getBytesPerPixel(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::Integer_RGBA:
        return 4;
    case ImageFormat::Float_R:
        return 4;
    case ImageFormat::Float_RGB:
        return 12;
    case ImageFormat::Float_RGBA:
        return 16;
    case ImageFormat::Half_RGBA:
        return 8;
    case ImageFormat::Short_RGBA:
        return 8;
    case ImageFormat::Integer_RG:
        return 8;
    case ImageFormat::Byte_R:
        return 1;
    }

    return 0;
}

// Gets the corresponding DXGI format for the specified IImage format, optionally with sRGB
// linearization.
// TODO: Move this function and the one above to a central location.
DXGI_FORMAT PTImage::getDXFormat(ImageFormat format, bool linearize)
{
    switch (format)
    {
    case ImageFormat::Integer_RGBA:
        return linearize ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::Float_R:
        return DXGI_FORMAT_R32_FLOAT;
    case ImageFormat::Float_RGB:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImageFormat::Float_RGBA:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImageFormat::Half_RGBA:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImageFormat::Short_RGBA:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case ImageFormat::Integer_RG:
        return DXGI_FORMAT_R32G32_UINT;
    case ImageFormat::Byte_R:
        return DXGI_FORMAT_R8_UNORM;
    }

    return DXGI_FORMAT_UNKNOWN;
}

void PTImage::createSRV(
    const PTRenderer& renderer, PTImage* pImage, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
    // Populate the SRV description.
    // NOTE: Even if this is going to be a null descriptor (see below), the description is needed
    // for unbound resources to work correctly when accessed, e.g. to return black.
    DXGI_FORMAT dxFormat = getDXFormat(
        pImage ? pImage->_format : ImageFormat::Integer_RGBA, pImage ? pImage->_linearize : false);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format                          = dxFormat;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels             = UINT_MAX;

    // Create the SRV with the specified descriptor handle.
    // NOTE: This will create a null descriptor if the image pointer is null.
    ID3D12Resource* pResource = pImage ? pImage->_pTexture.Get() : nullptr;
    renderer.dxDevice()->CreateShaderResourceView(pResource, &srvDesc, handle);
}

void PTImage::initResource(const void* pImageData)
{
    // Create the texture resource in GPU memory. This creates it in the default heap), with the
    // copy destination (write) state as the initial resource state.
    // NOTE: It is not possible to create a texture on an upload heap, for writing directly from the
    // CPU, so a separate copy must be performed later. It is possible write directly from the CPU
    // with a *custom* heap and Map+WriteToSubresource(), which may be optimal for integrated (UMA)
    // devices that use system memory.
    _pTexture = _pRenderer->createTexture(_dimensions, getDXFormat(_format), _name);

    // Create a temporary buffer with the image data, for upload to the GPU.
    size_t tempBufferSize              = ::GetRequiredIntermediateSize(_pTexture.Get(), 0, 1);
    ID3D12ResourcePtr pTempBuffer      = _pRenderer->createBuffer(tempBufferSize);
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData                  = pImageData;
    textureData.RowPitch               = getBytesPerPixel(_format) * _dimensions.x;
    textureData.SlicePitch             = textureData.RowPitch * _dimensions.y;

    // Copy the temporary buffer data to the texture using a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = _pRenderer->beginCommandList();
    ::UpdateSubresources(
        pCommandList.Get(), _pTexture.Get(), pTempBuffer.Get(), 0, 0, 1, &textureData);

    // Transition the texture's resource state from writing to (shader) reading, and complete the
    // task. We must wait for the task to be completed to ensure the temporary buffer is no longer
    // being used, and can therefore be released at the end of the function.
    _pRenderer->addTransitionBarrier(_pTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _pRenderer->submitCommandList();
    _pRenderer->completeTask();
    _pRenderer->waitForTask();
}

END_AURORA
