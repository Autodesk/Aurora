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

#include "EnvironmentBase.h"

BEGIN_AURORA

// Forward declarations.
class PTRenderer;

// An internal implementation for IEnvironment.
class PTEnvironment : public EnvironmentBase
{
public:
    /*** Lifetime Management ***/

    PTEnvironment(PTRenderer* pRenderer);
    ~PTEnvironment() {}

    /*** Functions ***/

    uint32_t descriptorCount() const
    {
        // Light texture and background texture.
        return 2;
    }
    ID3D12Resource* buffer() const { return _pConstantBuffer.Get(); }
    ID3D12Resource* aliasMap() const;
    bool update();
    void createDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const;

private:
    /*** Private Types ***/

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    ID3D12ResourcePtr _pConstantBuffer;
};
MAKE_AURORA_PTR(PTEnvironment);

END_AURORA