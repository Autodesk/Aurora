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

#include "PTEnvironment.h"

#include "PTImage.h"
#include "PTRenderer.h"

BEGIN_AURORA

PTEnvironment::PTEnvironment(PTRenderer* pRenderer) : _pRenderer(pRenderer) {}

ID3D12Resource* PTEnvironment::aliasMap() const
{
    // Get the light image and corresponding alias map.
    // NOTE: If the light image exists (or not), the alias map must also exist (or not).
    PTImagePtr pImage         = dynamic_pointer_cast<PTImage>(_values.asImage("light_image"));
    ID3D12Resource* pAliasMap = pImage ? pImage->aliasMap() : nullptr;
    assert((bool)pImage == (bool)pAliasMap);

    return pAliasMap;
}

bool PTEnvironment::update()
{
    // Do nothing if the environment is not dirty.
    if (!_bIsDirty)
    {
        return false;
    }

    // NOTE: Updating the constant buffer while it is being used can lead to ghosting artifacts
    // with progressive path tracing, i.e. some samples are using old data while others are using
    // new data, in the same task. This can be resolved by either flushing the renderer when the
    // environment is changed, or using a copy of the environment data for each task that is being
    // processed at once, as is done with per-frame data. With the latter option, that would have to
    // include the environment images.

    // Update the constant buffer with environment data, creating the buffer if needed. The lambda
    // here translates values to the data object for the constant buffer (GPU).
    _pRenderer->updateBuffer<EnvironmentData>(
        _pConstantBuffer, [this](EnvironmentData& data) { updateGPUStruct(data); });

    _bIsDirty = false;

    return true;
}

void PTEnvironment::createDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const
{
    // Create a SRV (descriptor) on the descriptor heap for the light image, if any.
    PTImagePtr pImage = dynamic_pointer_cast<PTImage>(_values.asImage("light_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the light_image SRV.
    handle.Offset(increment);

    // Create a SRV (descriptor) on the descriptor heap for the background image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("background_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the background_image SRV.
    handle.Offset(increment);
}

END_AURORA