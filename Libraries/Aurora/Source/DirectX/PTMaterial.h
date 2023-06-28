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

#include "MaterialBase.h"
#include "MemoryPool.h"

BEGIN_AURORA

// Forward declarations.
class PTRenderer;
class MaterialShader;
class PTShaderLibrary;
struct MaterialDefaultValues;

struct TextureTransform
{
    vec2 pivot;
    vec2 scale;
    vec2 offset;
    float rotation = 0.0f;
};

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

    PTMaterial(PTRenderer* pRenderer, MaterialShaderPtr pShader,
        shared_ptr<MaterialDefinition> pDef);
    ~PTMaterial() {};

    /*** Functions ***/

    // The total number of texture descriptors for each material instance.
    static uint32_t descriptorCount()
    {
        // Base color, specular roughness, normal, emission color, and opacity textures.
        return 5;
    }

    // The total number of sampler descriptors for each material instance.
    static uint32_t samplerDescriptorCount()
    {
        // Base color + opacity only for now.
        // TODO: Support samplers for other textures.
        return 2;
    }

    // Gets the constant buffer for this material.
    ID3D12Resource* buffer() const { return _constantBuffer.pGPUBuffer.Get(); }

    bool update();
    void createDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const;
    void createSamplerDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const;
    size_t computeSamplerHash() const;

private:
    /*** Private Types ***/

    /*** Private Functions ***/

    bool computeIsOpaque() const;

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    TransferBuffer _constantBuffer;
};
MAKE_AURORA_PTR(PTMaterial);

END_AURORA
