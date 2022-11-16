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
#include "HGIRenderer.h"

BEGIN_AURORA

// Forward declarations.
class HGIRenderer;

// An internal implementation for IWindow.
class HGIRenderBuffer : public IRenderBuffer
{
public:
    // Constructor and destructor.
    HGIRenderBuffer(HGIRenderer* pRenderer, uint32_t width, uint32_t height, ImageFormat format);
    ~HGIRenderBuffer() {}

    // IRenderBuffer function implementations.
    const void* data(size_t& stride, bool removePadding) override;

    // ITarget function implementations.
    void resize(uint32_t width, uint32_t height) override;

    IBufferPtr asReadable(size_t& /*stride*/) override { return nullptr; }

    IBufferPtr asShared() override { return nullptr; }

    pxr::HgiTextureHandle storageTex() { return _storageTex->handle(); }
    uint32_t width() { return _width; }
    uint32_t height() { return _height; }

private:
    HgiTextureHandleWrapper::Pointer _storageTex;
    HGIRenderer* _pRenderer;
    uint32_t _width;
    uint32_t _height;
    std::vector<uint8_t> _mappedBuffer;
};

END_AURORA