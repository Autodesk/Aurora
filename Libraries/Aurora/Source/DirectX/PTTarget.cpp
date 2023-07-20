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

#include "PTTarget.h"

#include "PTImage.h"
#include "PTRenderer.h"

BEGIN_AURORA

PTWindow::PTWindow(PTRenderer* pRenderer, WindowHandle handle, uint32_t width, uint32_t height) :
    PTTarget(pRenderer, width, height)
{
    // Initialize member variables.
    _pRenderer  = pRenderer;
    _dimensions = uvec2(width, height);

    // Get DirectX objects from the renderer.
    ID3D12Device5Ptr pDevice  = _pRenderer->dxDevice();
    IDXGIFactory4Ptr pFactory = _pRenderer->dxFactory();

    // Prepare a swap chain description.
    // NOTE: Tearing (for variable refresh rate displays, or VRR) is enabled for the swap chain.
    // This is always available with DX12 and Windows 10 after mid-2016. This actually takes effect
    // when vsync is disabled and with an appropriate display.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount           = _backBufferCount;
    swapChainDesc.Width                 = _dimensions.x;
    swapChainDesc.Height                = _dimensions.y;
    swapChainDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count      = 1;
    swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    // Create a swap chain.
    // NOTE: The creation function only accepts ID3D12SwapChain1, so we query for ID3D12SwapChain3.
    auto hwnd = static_cast<HWND>(handle);
    IDXGISwapChain1Ptr pSwapChain1;
    checkHR(pFactory->CreateSwapChainForHwnd(
        _pRenderer->commandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &pSwapChain1));
    pSwapChain1.As(&_pSwapChain);

    // Disable default handling of ALT-ENTER by DXGI to set the window into a full-screen exclusive
    // mode. Instead, an Aurora client should style and place the window to be a full-screen
    // borderless window. Full-screen exclusive mode is generally used for games, but it not
    // appropriate (or necessary) for Aurora.
    pFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create a descriptor heap for render target views.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors             = _backBufferCount;
    rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    checkHR(pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_pRTVHeap)));

    // Create render targets for the buffers in the swap chain.
    createBackBuffers();
}

void PTWindow::resize(uint32_t width, uint32_t height)
{
    assert(width > 0 && height > 0);

    // Do nothing if the swap chain dimensions have not changed.
    if (width == _dimensions.x && height == _dimensions.y)
    {
        return;
    }

    // Flush the renderer to make sure the swap chain is no longer being used, and release the
    // render targets.
    // NOTE: All references (both direct and indirect) to the swap chain buffers must be released
    // before the swap chain can be resized.
    _pRenderer->waitForTask();
    _backBuffers.clear();

    // Resize the swap chain, keeping the original buffer count and format. Also recreate the
    // render targets.
    _dimensions = uvec2(width, height);
    _pSwapChain->ResizeBuffers(
        0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
    createBackBuffers();
}
void PTWindow::copyFromResource(ID3D12Resource* pSource, ID3D12GraphicsCommandList4* pCommandList)
{
    // Get the resource for the current back buffer (target). The current back buffer changes on
    // each swap chain Present() call.
    ID3D12Resource* pBackBufferResource =
        _backBuffers[_pSwapChain->GetCurrentBackBufferIndex()].resource();

    // Transition the back buffer's resource from a presentable buffer to a copy destination, then
    // copy the renderer's result to the resource, and transition back to presenting.
    _pRenderer->addTransitionBarrier(
        pBackBufferResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    pCommandList->CopyResource(pBackBufferResource, pSource);
    _pRenderer->addTransitionBarrier(
        pBackBufferResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
}

void PTWindow::present()
{
    // Present the next buffer in the swap chain. Tearing (VRR) is set when vsync is disabled, i.e.
    // sync interval of zero.
    _pSwapChain->Present(_isVSyncEnabled ? 1 : 0, _isVSyncEnabled ? 0 : DXGI_PRESENT_ALLOW_TEARING);
}

void PTWindow::createBackBuffers()
{
    ID3D12Device5Ptr pDevice = _pRenderer->dxDevice();

    // Create render target views for each back buffer in the swap chain.
    // NOTE: A non "_SRGB" format is deliberately used here. The renderer may use CopyResource()
    // which does not perform gamma correction with a _SRGB RTV; only shader writes will do it. So
    // to keep consistent behavior, the renderer must perform any gamma correction; this is very
    // fast and simple to do anyway.
    _backBuffers.resize(_backBufferCount);
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM; // not _SRGB
    rtvDesc.Texture2D.MipSlice            = 0;
    UINT rtvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
    for (uint32_t i = 0; i < _backBufferCount; i++)
    {
        ID3D12ResourcePtr pBuffer;
        _pSwapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&pBuffer));
        pDevice->CreateRenderTargetView(pBuffer.Get(), &rtvDesc, rtvHandle);
        _backBuffers[i] = BackBuffer(pBuffer.Get(), rtvHandle);
        rtvHandle.Offset(rtvSize);
    }
}

PTRenderBuffer::PTRenderBuffer(
    PTRenderer* pRenderer, uint32_t width, uint32_t height, ImageFormat format) :
    PTTarget(pRenderer, width, height, format)
{
}

void PTRenderBuffer::resize(uint32_t width, uint32_t height)
{
    assert(width > 0 && height > 0);

    // Do nothing if the dimensions are unchanged.
    if (width == _dimensions.x && height == _dimensions.y)
    {
        return;
    }

    _dimensions = uvec2(width, height);

    // Wait for any previous work to be completed, so that the readback buffer is not released while
    // in use.
    _pRenderer->waitForTask();

    // Simply clear the host data buffers and the resources; they will be recreated as needed.
    _pData.clear();
    _pReadbackBuffer.Reset();
    _pShareableTexture.Reset();
    _sharedTextureHandle = nullptr;

    _hasPendingData = false;
}

class PTReadbackBuffer : public IRenderBuffer::IBuffer
{
public:
    PTReadbackBuffer(ID3D12ResourcePtr pReadbackBuffer, size_t stride, size_t bytesPerPixel) :
        _pReadbackBuffer(pReadbackBuffer), _stride(stride), _bytesPerPixel(bytesPerPixel)
    {
        if (pReadbackBuffer)
            _pReadbackBuffer->Map(0, nullptr, &_pSrcData);
    }

    ~PTReadbackBuffer() override
    {
        if (_pReadbackBuffer)
        {
            CD3DX12_RANGE writeRange(0, 0);
            _pReadbackBuffer->Unmap(0, &writeRange); // no HRESULT
        }
    }

    const void* data() override { return _pSrcData; }

    const void data(size_t width, uint32_t height, void* pDataOut)
    {
        if (!_pReadbackBuffer)
            return;

        // Pointer to start of row in source data
        byte* srcPtr  = static_cast<byte*>(_pSrcData);
        byte* destPtr = static_cast<byte*>(pDataOut);

        size_t widthInBytes = width * _bytesPerPixel;
        if (widthInBytes == _stride)
        {
            // faster path to copy the whole buffer
            ::memcpy_s(destPtr, _stride * height, srcPtr, _stride * height);
        }
        else
        {
            // End of the destination buffer
            byte* destPtrEnd = destPtr + (widthInBytes * height);

            // Copy the image row by row (excluding the padding)
            for (size_t i = 0; i < height; i++)
            {
                // Calculate remaining bytes (required by memcpy_s to avoid buffer overruns)
                size_t remBytes = destPtrEnd - destPtr;
                // Copy start of current row (skipping any padding at the end)
                ::memcpy_s(destPtr, remBytes, srcPtr, widthInBytes);
                // Advance to next row in destination data (with no padding)
                destPtr += widthInBytes;
                // Advance to next row in source data (with padding)
                srcPtr += _stride;
            }
        }
    }

private:
    void* _pSrcData = nullptr;
    ID3D12ResourcePtr _pReadbackBuffer;
    size_t _stride, _bytesPerPixel;
};

size_t PTRenderBuffer::bytesPerPixel()
{
    return PTImage::getBytesPerPixel(_format);
}

IRenderBuffer::IBufferPtr PTRenderBuffer::asReadable(size_t& stride)
{
    // set the row stride
    stride = _dataStride;

    // Wait for any previous work to be completed, so that the copy to the readback buffer has
    // the pending data.
    _pRenderer->waitForTask();

    return std::make_shared<PTReadbackBuffer>(_pReadbackBuffer, _dataStride, bytesPerPixel());
}

const void* PTRenderBuffer::data(size_t& stride, bool removePadding)
{
    // compute the row stride
    stride = removePadding ? (size_t)(bytesPerPixel() * _dimensions.x) : _dataStride;

    if (_hasPendingData)
    {
        // Initialize the host data buffer if needed.
        // NOTE: The _dataSize member (i.e. the "total bytes" property from GetCopyableFootprints())
        // excludes the padding for the last row, but we define the host data buffer to include that
        // padding here, i.e. a full stride for every row, as that is what the client will expect.
        if (_pData.empty())
            _pData.resize(stride * _dimensions.y);

        // Read the entire contents of the readback buffer into the host data buffer.
        IBufferPtr readback             = asReadable(stride);
        PTReadbackBuffer* privateBuffer = static_cast<PTReadbackBuffer*>(readback.get());
        privateBuffer->data(_dimensions.x, _dimensions.y, _pData.data());

        _hasPendingData = false;
    }

    // Return the data pointer.
    return _pData.data();
}

void PTRenderBuffer::copyFromResource(
    ID3D12Resource* pSource, ID3D12GraphicsCommandList4* pCommandList)
{
    // Determine the size and stride of the source data, as well as information for copying from it.
    D3D12_RESOURCE_DESC desc = pSource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    _pRenderer->dxDevice()->GetCopyableFootprints(
        &desc, 0, 1, 0, &layout, nullptr, nullptr, &_dataSize);
    _dataStride = layout.Footprint.RowPitch;

    // Create the readback buffer if needed.
    // NOTE: It is not possible to use a texture buffer for readback, so a plain buffer is used.
    if (!_pReadbackBuffer)
    {
        _pReadbackBuffer = _pRenderer->createBuffer(_dataSize, "Readback Buffer",
            D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    }

    // Use the command list to add a command to copy from the source texture to the readback buffer.
    CD3DX12_TEXTURE_COPY_LOCATION copyBufferDest(_pReadbackBuffer.Get(), layout);
    CD3DX12_TEXTURE_COPY_LOCATION copySrc(pSource);
    pCommandList->CopyTextureRegion(&copyBufferDest, 0, 0, 0, &copySrc, nullptr);

    // Create a shareable texture
    if (!_pShareableTexture)
    {
        _pShareableTexture = _pRenderer->createTexture(this->_dimensions,
            PTImage::getDXFormat(this->format()), "Shared Result Texture", false, true);

        // Create a shared handle for the shareable texture
        auto hr = _pRenderer->dxDevice()->CreateSharedHandle(
            _pShareableTexture.Get(), NULL, GENERIC_ALL, NULL, &_sharedTextureHandle);
        if (hr != S_OK)
        {
            _sharedTextureHandle = nullptr;
            AU_WARN("Failed to create a shared handle for PTRenderBuffer");
        }
    }
    // Use the command list to add a command to copy from the source texture to the shareable
    // texture.
    CD3DX12_TEXTURE_COPY_LOCATION copyTextureDest(_pShareableTexture.Get());
    pCommandList->CopyTextureRegion(&copyTextureDest, 0, 0, 0, &copySrc, nullptr);

    _hasPendingData = true;
}

END_AURORA
