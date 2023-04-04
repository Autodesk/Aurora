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

class HdAuroraRenderDelegate;

#include <pxr/imaging/hgi/texture.h>
class HdAuroraRenderBuffer : public HdRenderBuffer
{
public:
    HdAuroraRenderBuffer(SdfPath const& id, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraRenderBuffer();

    void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam,
        HdDirtyBits* dirtyBits) override;

    void Finalize(HdRenderParam* renderParam) override;

    bool Allocate(GfVec3i const& dimensions, HdFormat format, bool multiSampled) override;

    unsigned int GetWidth() const override { return _dimensions[0]; }

    unsigned int GetHeight() const override { return _dimensions[1]; }

    unsigned int GetDepth() const override { return _dimensions[2]; }

    HdFormat GetFormat() const override { return _format; }

    bool IsMultiSampled() const override { return false; }

    void* Map() override;

    void Unmap() override { _mapped = false; }

    bool IsMapped() const override { return _mapped; }

    bool IsConverged() const override;

    void Resolve() override;

    VtValue GetResource(bool multiSampled) const override;

    // Has the underlying Aurora object been allocated?
    bool IsAllocated() const { return _pRenderBuffer != nullptr; }

    // Get the Aurora object for this render buffer
    Aurora::IRenderBufferPtr GetAuroraBuffer() { return _pRenderBuffer; }

    // Set the converged flag on render buffer (called from render pass)
    void SetConverged(bool val) { _converged = val; }

    // Get the stride of the render buffer
    size_t GetStride() { return _stride; }

private:
    virtual void _Deallocate() override;

    bool hasTextureResource(const pxr::HgiTextureDesc& texDesc);

    HdAuroraRenderDelegate* _owner;

    Aurora::IRenderBufferPtr _pRenderBuffer;
    pxr::HgiTextureHandle _texture;
    GfVec3i _dimensions;
    size_t _stride;
    bool _mapped;
    HdFormat _format;
    bool _converged;
    bool _valid;
};
