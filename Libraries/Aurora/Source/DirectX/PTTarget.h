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
#pragma once

BEGIN_AURORA

// Forward declarations.
class PTRenderer;

// An internal implementation for ITarget.
class PTTarget
{
public:
    /*** Lifetime Management ***/

    PTTarget(PTRenderer* pRenderer, uint32_t width, uint32_t height,
        ImageFormat format = ImageFormat::Integer_RGBA) :
        _pRenderer(pRenderer), _dimensions(width, height), _format(format)
    {
        assert(width > 0 && height > 0);
    }

    /*** Functions ***/

    virtual const uvec2& dimensions() const { return _dimensions; };
    virtual void copyFromResource(
        ID3D12Resource* pSource, ID3D12GraphicsCommandList4* pCommandList) = 0;
    virtual void present() {};

    /// Returns the format of the target.
    ImageFormat format() { return _format; }

protected:
    /*** Protected Variables ***/

    PTRenderer* _pRenderer = nullptr;
    uvec2 _dimensions;
    ImageFormat _format;
};
MAKE_AURORA_PTR(PTTarget);

// An internal implementation for IWindow.
class PTWindow : public IWindow, public PTTarget
{
public:
    /*** Lifetime Management ***/

    PTWindow(PTRenderer* pRenderer, WindowHandle handle, uint32_t width, uint32_t height);

    /*** ITarget Functions ***/

    void resize(uint32_t width, uint32_t height) override;

    /*** IWindow Functions ***/

    void setVSyncEnabled(bool enabled) override { _isVSyncEnabled = enabled; }

    /*** PTTarget Functions ***/

    void copyFromResource(
        ID3D12Resource* pSource, ID3D12GraphicsCommandList4* pCommandList) override;
    void present() override;

private:
    /*** Private Types ***/

    class BackBuffer
    {
    public:
        BackBuffer() {};
        BackBuffer(ID3D12Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
        {
            _pResource = pResource;
            _rtvHandle = rtvHandle;
        }
        ID3D12Resource* resource() const { return _pResource.Get(); }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle() const { return _rtvHandle; }

    private:
        ID3D12ResourcePtr _pResource;
        D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle = {};
    };

    /*** Private Functions ***/

    void createBackBuffers();

    /*** Private Variables ***/

    uint32_t _backBufferCount = 3;
    IDXGISwapChain3Ptr _pSwapChain;
    ID3D12DescriptorHeapPtr _pRTVHeap;
    vector<BackBuffer> _backBuffers;
    bool _isVSyncEnabled = false;
};
MAKE_AURORA_PTR(PTWindow);

// Implementation of IBuffer that just contains a reference to the shared handle for the texture
class PTSharedMemory : public IRenderBuffer::IBuffer
{
public:
    PTSharedMemory() = delete;

    // Handle to a buffer created with CreateSharedHandle
    explicit PTSharedMemory(HANDLE sharedMemoryHandle) : _sharedMemoryHandle(sharedMemoryHandle) {}

    ~PTSharedMemory() override {}

    const void* data() override { return nullptr; }
    const void* handle() override { return _sharedMemoryHandle; }

private:
    HANDLE _sharedMemoryHandle;
};
MAKE_AURORA_PTR(PTSharedMemory);

// An internal implementation for IRenderBuffer.
class PTRenderBuffer : public IRenderBuffer, public PTTarget
{
public:
    /*** Lifetime Management ***/

    PTRenderBuffer(PTRenderer* pRenderer, uint32_t width, uint32_t height, ImageFormat format);

    /*** ITarget Functions ***/

    void resize(uint32_t width, uint32_t height) override;

    /*** IRenderBuffer Functions ***/

    const void* data(size_t& stride, bool removePadding) override;
    IBufferPtr asReadable(size_t& stride) override;
    IBufferPtr asShared() override
    {
        return std::make_shared<PTSharedMemory>(_sharedTextureHandle);
    }

    /*** PTTarget Functions ***/

    void copyFromResource(
        ID3D12Resource* pSource, ID3D12GraphicsCommandList4* pCommandList) override;

private:
    size_t bytesPerPixel();

    /*** Private Variables ***/

    bool _hasPendingData = false;
    vector<byte> _pData;
    size_t _dataSize   = 0;
    size_t _dataStride = 0;

    // Buffer for data readback
    ID3D12ResourcePtr _pReadbackBuffer;

    // Shareable texture representation of the source texture
    ID3D12ResourcePtr _pShareableTexture;

    // Shared handle to the shareable texture
    HANDLE _sharedTextureHandle = nullptr;
};

END_AURORA