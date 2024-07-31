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

#include "HdAuroraRenderBuffer.h"
#include "HdAuroraRenderDelegate.h"

#include <GL/glew.h>
#include <pxr/imaging/hdx/hgiConversions.h>
#include <pxr/imaging/hgi/blitCmds.h>
#include <pxr/imaging/hgi/blitCmdsOps.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/types.h>

#include <atomic>
static std::atomic<uint64_t> gUniqueRenderBufferIdCounter(1);

void HdAuroraPostPendingGLErrors()
{
    GLenum error;
    // Protect from doing infinite looping when glGetError
    // is called from an invalid context.
    int watchDogCount = 0;
    while ((watchDogCount++ < 256) && ((error = glGetError()) != GL_NO_ERROR))
    {
        std::ostringstream errorMessage;
        errorMessage << "GL error code: 0x" << std::hex << error << std::dec;
        TF_RUNTIME_ERROR(errorMessage.str());
    }
}

// An OpenGL GPU texture resource.
class HgiHdAuroraTextureGL final : public HgiTexture
{
public:
    HgiHdAuroraTextureGL(HgiTextureDesc const& desc, void* sharedHandle);

    ~HgiHdAuroraTextureGL() override;

    size_t GetByteSizeOfResource() const override
    {
        GfVec3i const& s = _descriptor.dimensions;
        return HgiGetDataSizeOfFormat(_descriptor.format) * s[0] * s[1] * std::max(s[2], 1);
    }

    uint64_t GetRawResource() const override { return (uint64_t)_textureId; }

    /// Returns the OpenGL id / name of the texture.
    uint32_t GetTextureId() const { return _textureId; }

    void SubmitLayoutChange(HgiTextureUsage /*newLayout*/) override { return; }

private:
    HgiHdAuroraTextureGL()                                       = delete;
    HgiHdAuroraTextureGL& operator=(const HgiHdAuroraTextureGL&) = delete;
    HgiHdAuroraTextureGL(const HgiHdAuroraTextureGL&)            = delete;

    uint32_t _textureId;
};

HgiHdAuroraTextureGL::HgiHdAuroraTextureGL(
    HgiTextureDesc const& desc, [[maybe_unused]] void* sharedHandle) :
    HgiTexture(desc), _textureId(0)
{
    if (desc.layerCount > 1)
    {
        // XXX Further below we are missing support for layered textures.
        TF_CODING_ERROR("XXX Missing implementation for texture arrays");
    }

    GLenum glInternalFormat = 0;

    if (desc.usage & HgiTextureUsageBitsDepthTarget)
    {
        TF_VERIFY(desc.format == HgiFormatFloat32 || desc.format == HgiFormatFloat32UInt8);

        if (desc.format == HgiFormatFloat32UInt8)
        {
            glInternalFormat = GL_DEPTH32F_STENCIL8;
        }
        else
        {
            glInternalFormat = GL_DEPTH_COMPONENT32F;
        }
    }
    else
    {
        glInternalFormat = GL_RGBA16F;
    }

    if (desc.sampleCount == HgiSampleCount1)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
    }
    else
    {
        if (desc.type != HgiTextureType2D)
        {
            TF_CODING_ERROR("Only 2d multisample textures are supported");
        }
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &_textureId);
    }

    if (!_descriptor.debugName.empty())
    {
        glObjectLabel(GL_TEXTURE, _textureId, -1, _descriptor.debugName.c_str());
    }

    if (desc.sampleCount == HgiSampleCount1)
    {
        // XXX sampler state etc should all be set via tex descriptor.
        //     (probably pass in HgiSamplerHandle in tex descriptor)
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        const uint16_t mips = desc.mipLevels;
        GLint minFilter     = mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
        glTextureParameteri(_textureId, GL_TEXTURE_MIN_FILTER, minFilter);
        glTextureParameteri(_textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        float aniso = 2.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
        glTextureParameterf(_textureId, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        glTextureParameteri(_textureId, GL_TEXTURE_BASE_LEVEL, /*low-mip*/ 0);
        glTextureParameteri(_textureId, GL_TEXTURE_MAX_LEVEL, /*hi-mip*/ mips - 1);

        // gl needs memory to be aligned at pow2 row sizes.
        int nextPow2 = 1;
        while (nextPow2 < desc.dimensions[0])
            nextPow2 *= 2;
        size_t bytesPerPixel = HgiGetDataSizeOfFormat(_descriptor.format);

        // Create an OpenGL memory object and sync the address of the shared GPU pointer from
        // Aurora to this gl memory address.  (DX12 and GL will use the same render buffer)
        GLuint glData = 0;
        glCreateMemoryObjectsEXT(1, &glData);

#if defined(WIN32)
        // The correct enum here appears to be GL_HANDLE_TYPE_D3D12_RESOURCE_EXT but on AMD cards
        // the glImportMemoryWin32HandleEXT call reports an Invalid Enum so we use
        // GL_HANDLE_TYPE_OPAQUE_WIN32_EXT on AMD cards instead.
        GLenum handleType = GL_HANDLE_TYPE_D3D12_RESOURCE_EXT;
        std::string vendorString((char*)glGetString(GL_VENDOR));

        // GL_VENDOR "ATI Technologies Inc."
        if (vendorString.length() >= 3 && vendorString.substr(0, 3) == "ATI")
            handleType = GL_HANDLE_TYPE_OPAQUE_WIN32_EXT;
        glImportMemoryWin32HandleEXT(
            glData, nextPow2 * bytesPerPixel * desc.dimensions[1], handleType, sharedHandle);
#else
        // TODO: Implement for other platforms to support Vulkan/Metal
        (void)glData;
        (void)bytesPerPixel;
        (void)nextPow2;
        AU_FAIL("HgiHdAuroraTexture not supported on this platform.");
#endif
        glTextureStorageMem2DEXT(
            _textureId, 1, glInternalFormat, desc.dimensions[0], desc.dimensions[1], glData, 0);

        // Check for errors after making OpenGL calls.
        HdAuroraPostPendingGLErrors();
    }
    else
    {
        // Note: Setting sampler state values on multi-sample texture is invalid
        glTextureStorage2DMultisample(_textureId, desc.sampleCount, glInternalFormat,
            desc.dimensions[0], desc.dimensions[1], GL_TRUE);
    }

    HdAuroraPostPendingGLErrors();
}

HgiHdAuroraTextureGL::~HgiHdAuroraTextureGL()
{
    if (_textureId > 0)
    {
        glDeleteTextures(1, &_textureId);
        _textureId = 0;
    }

    HdAuroraPostPendingGLErrors();
}

// An DirectX GPU texture resource, using a shared handled.
class HgiHdAuroraTextureDX final : public HgiTexture
{
public:
    HgiHdAuroraTextureDX(HgiTextureDesc const& desc, void* sharedDXHandle) :
        HgiTexture(desc), _sharedDXHandle(sharedDXHandle)
    {
    }

    ~HgiHdAuroraTextureDX() override {}

    size_t GetByteSizeOfResource() const override
    {
        GfVec3i const& s = _descriptor.dimensions;
        return HgiGetDataSizeOfFormat(_descriptor.format) * s[0] * s[1] * std::max(s[2], 1);
    }

    uint64_t GetRawResource() const override { return reinterpret_cast<uint64_t>(_sharedDXHandle); }

    void SubmitLayoutChange(HgiTextureUsage /*newLayout*/) override { return; }

private:
    HgiHdAuroraTextureDX()                                       = delete;
    HgiHdAuroraTextureDX& operator=(const HgiHdAuroraTextureDX&) = delete;
    HgiHdAuroraTextureDX(const HgiHdAuroraTextureDX&)            = delete;

    void* _sharedDXHandle;
};

// 1x1 pixel of invalid data
static const uint8_t INVALID_DATA[] = { 0xFF, 0xFF, 0xFF, 0xFF };

// Obtain appropriate usage bits according to the format and purpose of the texture.
static HgiTextureUsage _getTextureUsage(HdFormat format, TfToken const& name)
{
    if (HdAovHasDepthSemantic(name))
    {
        if (format == HdFormatFloat32UInt8 || format == HdFormatFloat32)
        {
            return HgiTextureUsageBitsDepthTarget | HgiTextureUsageBitsStencilTarget;
        }
        return HgiTextureUsageBitsDepthTarget;
    }

    return HgiTextureUsageBitsColorTarget;
}

HdAuroraRenderBuffer::HdAuroraRenderBuffer(
    SdfPath const& id, HdAuroraRenderDelegate* renderDelegate) :
    HdRenderBuffer(id),
    _owner(renderDelegate),
    _pRenderBuffer(nullptr),
    _dimensions(0, 0, 1),
    _stride(0),
    _format(HdFormatInvalid),
    _converged(false),
    _valid(true) // Uninitialized render buffers are considered valid
{
}

HdAuroraRenderBuffer::~HdAuroraRenderBuffer()
{
    _pRenderBuffer = nullptr;
}

void HdAuroraRenderBuffer::Sync(
    HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    HdRenderBuffer::Sync(sceneDelegate, renderParam, dirtyBits);
}

void HdAuroraRenderBuffer::Finalize(HdRenderParam* /* renderParam */)
{
    _Deallocate();
}

void HdAuroraRenderBuffer::_Deallocate()
{
    _pRenderBuffer.reset();
    _dimensions = GfVec3i(0, 0, 1);
    _stride     = 0;
    _format     = HdFormatUNorm8Vec4;
    _converged  = false;
    _valid      = true;
}

void* HdAuroraRenderBuffer::Map()
{

    _mapped = true;

    // Return a single pixel of invalid data if the buffer is not valid (in this case dimensions
    // will always 1x1x1) Hydra will create a bunch of invalid buffers for currently unsupported AOV
    // types (so we need to return *something* in this case)
    if (!_valid)
        return (void*)INVALID_DATA;

    if (!_pRenderBuffer)
        return nullptr;
    return (void*)_pRenderBuffer->data(_stride, true);
}

bool HdAuroraRenderBuffer::IsConverged() const
{
    // Invalid render buffers are considered always converged
    return !_valid || _converged;
}

bool HdAuroraRenderBuffer::Allocate(
    GfVec3i const& dimensions, HdFormat format, bool /* multiSampled */)
{
    // TODO We should not be creating render buffers with invalid formats.
    // Happens with depth and other AOVs.
    if (format == HdFormatInvalid)
    {
        _valid      = false;
        _format     = HdFormatUNorm8Vec4;
        _dimensions = GfVec3i(1, 1, 1);
        return false;
    }

    _valid      = true;
    _dimensions = dimensions;

    Aurora::ImageFormat imageFormat;
    switch (format)
    {
    case HdFormatUNorm8Vec4:
        imageFormat = Aurora::ImageFormat::Integer_RGBA;
        break;
    case HdFormatInt32:
        imageFormat = Aurora::ImageFormat::Integer_RG;
        break;
    case HdFormatFloat32:
        imageFormat = Aurora::ImageFormat::Float_R;
        break;
    default:
        imageFormat = Aurora::ImageFormat::Half_RGBA;
        break;
    }

    _pRenderBuffer =
        _owner->GetRenderer()->createRenderBuffer(dimensions[0], dimensions[1], imageFormat);

    // Get the stride TODO: not an ideal way to get the stride, but I don't think we need it anyway.
    _pRenderBuffer->asReadable(_stride);
    _format    = format;
    _converged = false;
    return true;
}

void HdAuroraRenderBuffer::Resolve()
{
    if (!_valid)
        return;

    if (!_pRenderBuffer)
        return;

    // Create and populate the HGI texture description
    pxr::HgiTextureDesc texDesc;
    texDesc.debugName      = "HdAurora AovInput Texture";
    texDesc.dimensions     = pxr::GfVec3i(GetWidth(), GetHeight(), 1);
    texDesc.format         = pxr::HdxHgiConversions::GetHgiFormat(GetFormat());
    texDesc.initialData    = nullptr;
    texDesc.layerCount     = 1;
    texDesc.mipLevels      = 1;
    texDesc.pixelsByteSize = 0;
    texDesc.sampleCount    = pxr::HgiSampleCount1;
    texDesc.usage =
        _getTextureUsage(GetFormat(), GetId().GetNameToken()) | pxr::HgiTextureUsageBitsShaderRead;

    // Shared buffers are only supported on DX12 (and therefore Windows) for now.
#if defined(WIN32)
    bool sharedBuffersSupported = _owner->GetRenderBufferSharingType() !=
        HdAuroraRenderDelegate::RenderBufferSharingType::NONE;
#else
    bool sharedBuffersSupported = false;
#endif

    // Try and get a shared buffer
    if (sharedBuffersSupported)
    {
        Aurora::IRenderBuffer::IBufferPtr sharedBuffer = _pRenderBuffer->asShared();
        if (sharedBuffer)
        {
            // If we have a shareable buffer then we create a special HdAurora supplied version of
            // an HgiTexture that handles the syncing of DX12 and GL memory.
            if (!hasTextureResource(texDesc))
            {
                auto* sharedHandle = const_cast<void*>(sharedBuffer->handle());
                if (_owner->GetRenderBufferSharingType() ==
                    HdAuroraRenderDelegate::RenderBufferSharingType::DIRECTX_TEXTURE_HANDLE)
                {
                    // Create a HgiTextureHandle that wraps a DirectX shareable texture handle
                    _texture = HgiTextureHandle(new HgiHdAuroraTextureDX(texDesc, sharedHandle),
                        gUniqueRenderBufferIdCounter.fetch_add(1));
                }
                else
                {
                    // Create a HgiTextureHandle that creates and wraps an OpenGL texture id
                    _texture = HgiTextureHandle(new HgiHdAuroraTextureGL(texDesc, sharedHandle),
                        gUniqueRenderBufferIdCounter.fetch_add(1));
                }
            }

            // We need to wait for Aurora to finish copying its result into the shared buffer before
            // we continue.
            _owner->GetRenderer()->waitForTask();

            // No more work to do if they are shared.  Buffers are synced automatically.
            return;
        }
    }

    // If we don't have a shareable buffer then fallback to using HGI blit
    size_t stride;
    auto pixelBuffer = _pRenderBuffer->asReadable(stride);
    if (!pixelBuffer)
        return;

    const void* pixelData = pixelBuffer->data();
    if (!pixelData)
        return;

    size_t bytesPerPixel = 16;
    switch (GetFormat())
    {
    case pxr::HdFormatFloat32:
    case pxr::HdFormatUNorm8Vec4:
        bytesPerPixel = 4;
        break;
    case pxr::HdFormatFloat16Vec4:
        bytesPerPixel = 8;
        break;
    case pxr::HdFormatUNorm8:
    case pxr::HdFormatUNorm8Vec2:
    case pxr::HdFormatUNorm8Vec3:
    case pxr::HdFormatSNorm8:
    case pxr::HdFormatSNorm8Vec2:
    case pxr::HdFormatSNorm8Vec3:
    case pxr::HdFormatSNorm8Vec4:
    case pxr::HdFormatFloat16:
    case pxr::HdFormatFloat16Vec2:
    case pxr::HdFormatFloat16Vec3:
    case pxr::HdFormatFloat32Vec2:
    case pxr::HdFormatFloat32Vec3:
    case pxr::HdFormatFloat32Vec4:
    case pxr::HdFormatUInt16:
    case pxr::HdFormatUInt16Vec2:
    case pxr::HdFormatUInt16Vec3:
    case pxr::HdFormatUInt16Vec4:
    case pxr::HdFormatInt16:
    case pxr::HdFormatInt16Vec2:
    case pxr::HdFormatInt16Vec3:
    case pxr::HdFormatInt16Vec4:
    case pxr::HdFormatInt32:
    case pxr::HdFormatInt32Vec2:
    case pxr::HdFormatInt32Vec3:
    case pxr::HdFormatInt32Vec4:
    case pxr::HdFormatFloat32UInt8:
    case pxr::HdFormatInvalid:
    case pxr::HdFormatCount:
        break;
    }

    int actualWidth = static_cast<int>(stride / bytesPerPixel);

    pxr::GfVec3i dim(actualWidth, static_cast<int>(GetHeight()), 1);

    // populate with data from render buffer
    texDesc.dimensions     = dim;
    texDesc.initialData    = pixelData;
    texDesc.pixelsByteSize = stride * GetHeight();

    bool hasTexture = static_cast<bool>(_texture);
    if (hasTexture && texDesc != _texture->GetDescriptor())
    {
        _owner->GetHgi()->DestroyTexture(&_texture);
        hasTexture = false;
    }
    if (!hasTexture)
    {
        _texture = _owner->GetHgi()->CreateTexture(texDesc);
    }
    else
    {
        // Update the GL texture in place.
        HgiBlitCmdsUniquePtr blitCmds = _owner->GetHgi()->CreateBlitCmds();
        HgiTextureCpuToGpuOp blitOp;
        blitOp.gpuDestinationTexture  = _texture;
        blitOp.mipLevel               = 0;
        blitOp.destinationTexelOffset = GfVec3i(0, 0, 0);
        blitOp.cpuSourceBuffer        = pixelData;
        blitCmds->CopyTextureCpuToGpu(blitOp);
        _owner->GetHgi()->SubmitCmds(blitCmds.get());
    }
}

VtValue HdAuroraRenderBuffer::GetResource(bool /*multiSampled*/) const
{
    if (!_valid)
        return VtValue();

    if (!_pRenderBuffer || !_texture)
        return VtValue();

    return VtValue(_texture);
}

bool HdAuroraRenderBuffer::hasTextureResource(const pxr::HgiTextureDesc& texDesc)
{
    // if we have a texture, but the required description had changed (aka changed size), then we
    // destroy the existing texture and recreate a new one.
    bool hasTexture = static_cast<bool>(_texture);
    if (hasTexture && texDesc != _texture->GetDescriptor())
    {
        _owner->GetHgi()->DestroyTexture(&_texture);
        hasTexture = false;
    }

    return hasTexture;
}
