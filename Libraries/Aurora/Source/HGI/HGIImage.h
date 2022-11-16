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

#include "HGIHandleWrapper.h"
#include "ImageBase.h"

BEGIN_AURORA

// Forward declarations.
class HGIRenderer;

// An internal implementation for IImage.
class HGIImage : public ImageBase
{
public:
    HGIImage(HGIRenderer* pRenderer, const IImage::InitData& initData);
    ~HGIImage() {};
    const pxr::HgiTextureHandle& texture() { return _texture->handle(); }
    pxr::HgiFormat getHGIFormat(ImageFormat format, bool linearize, uint32_t* pPixelByteSizeOut);

private:
    HgiTextureHandleWrapper::Pointer _texture;
};
MAKE_AURORA_PTR(HGIImage);

END_AURORA
