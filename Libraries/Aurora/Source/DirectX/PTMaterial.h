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

#include "MaterialBase.h"

BEGIN_AURORA

// Forward declarations.
class PTRenderer;
class PTMaterialType;
class PTShaderLibrary;

// An internal implementation for IMaterial.
class PTMaterial : public MaterialBase
{
public:
    /*** Static Functions ***/

    /// Validates that the offsets with CPU material structure match the GPU version in shader.
    ///
    /// \return True if valid.
    static bool validateOffsets(const PTShaderLibrary& pShaderLibrary);

    /*** Lifetime Management ***/

    PTMaterial(PTRenderer* pRenderer, shared_ptr<PTMaterialType> pType);
    ~PTMaterial() {};

    /*** Functions ***/

    // The total number of texture descriptors for each material instance.
    static uint32_t descriptorCount()
    {
        // Base color, specular roughness, normal and opacity textures.
        return 4;
    }

    // The total number of sampler descriptors for each material instance.
    static uint32_t samplerDescriptorCount()
    {
        // Base color + opacity only for now.
        return 2;
    }

    // Gets the constant buffer for this material.
    ID3D12Resource* buffer() const { return _pConstantBuffer.Get(); }

    // Gets the material type for this material.
    shared_ptr<PTMaterialType> materialType() const { return _pType; }

    bool update();
    void createDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const;
    void createSamplerDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const;
    size_t computeSamplerHash() const;

private:
    /*** Private Types ***/

    /*** Private Functions ***/

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    ID3D12ResourcePtr _pConstantBuffer;
    shared_ptr<PTMaterialType> _pType;
};
MAKE_AURORA_PTR(PTMaterial);

END_AURORA
