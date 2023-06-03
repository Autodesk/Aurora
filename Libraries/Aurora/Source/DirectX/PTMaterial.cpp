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

#include "PTMaterial.h"

#include "PTImage.h"
#include "PTRenderer.h"
#include "PTShaderLibrary.h"

BEGIN_AURORA

// Convenience macro to compare reflection data from shader to CPU offset
// NOTE: Can only be used inside PTMaterial::validateOffsets().
#define VALIDATE_OFFSET(_member)                                                                   \
    if (structDescriptions[#_member].Offset != uniformBuffer.getOffsetForVariable(#_member))       \
    {                                                                                              \
        AU_ERROR("%s offset mismatch: %d!=%d\n", #_member, structDescriptions[#_member].Offset,   \
            uniformBuffer.getOffsetForVariable(#_member));                                         \
        isValid = false;                                                                           \
    }

bool PTMaterial::validateOffsets(const PTShaderLibrary& pShaderLibrary)
{
    ComPtr<ID3D12LibraryReflection> pShaderLibraryReflection = pShaderLibrary.reflection();

    // Get the first function in library.
    auto pFuncRefl = pShaderLibraryReflection->GetFunctionByIndex(0);

    // Get the reflection data for constant buffer
    auto pCBRefl = pFuncRefl->GetConstantBufferByName("gMaterialConstants");

    // If we fail to get reflection data just abandon validation (should not be failure case.)
    if (!pCBRefl)
        return true;

    // Get the description of the constant buffer (which includes size).
    D3D12_SHADER_BUFFER_DESC cbDesc;
    pCBRefl->GetDesc(&cbDesc);

    // Get the first variable in the constant buffer.
    // NOTE:this is actually the struct representing the members.
    ID3D12ShaderReflectionVariable* pStructRefl = pCBRefl->GetVariableByIndex(0);

    // If we fail to get reflection data just abandon validation (should not be failure case.)
    if (!pStructRefl)
        return true;

    // Get the type for the struct, and its description.
    ID3D12ShaderReflectionType* pStructType = pStructRefl->GetType();

    // If we fail to get reflection struct data just abandon validation (should not be failure
    // case.)
    if (!pStructType)
        return true;

    // Get description of struct.
    // If the GetDesc call fails, which will happen if the struct is not referenced, just abandon
    // validation (should not be failure case.)
    D3D12_SHADER_TYPE_DESC structDesc;
    if (pStructType->GetDesc(&structDesc) != S_OK)
        return true;

    // Create a map of member name to description.
    map<string, D3D12_SHADER_TYPE_DESC> structDescriptions;

    // Iterate through the members in the struct.
    for (unsigned int i = 0; i < structDesc.Members; i++)
    {
        // Get the name and type of member.
        const char* pVarName                 = pStructType->GetMemberTypeName(i);
        ID3D12ShaderReflectionType* pVarType = pStructType->GetMemberTypeByIndex(i);

        // Get the description from the type
        D3D12_SHADER_TYPE_DESC varDesc;
        pVarType->GetDesc(&varDesc);

        // Add it to the map.
        structDescriptions[pVarName] = varDesc;
    }

    UniformBuffer uniformBuffer(StandardSurfaceUniforms, StandardSurfaceDefaults.properties);
    // AU_INFO("%s", uniformBuffer.generateHLSLStruct().c_str());
    // Validate each offset; isValid flag will get set to false if invalid.
    bool isValid = true;
    VALIDATE_OFFSET(base);
    VALIDATE_OFFSET(baseColor);
    VALIDATE_OFFSET(diffuseRoughness);
    VALIDATE_OFFSET(metalness);
    VALIDATE_OFFSET(specular);
    VALIDATE_OFFSET(specularColor);
    VALIDATE_OFFSET(specularRoughness);
    VALIDATE_OFFSET(specularIOR);
    VALIDATE_OFFSET(specularAnisotropy);
    VALIDATE_OFFSET(specularRotation);
    VALIDATE_OFFSET(transmission);
    VALIDATE_OFFSET(transmissionColor);
    VALIDATE_OFFSET(subsurface);
    VALIDATE_OFFSET(subsurfaceColor);
    VALIDATE_OFFSET(subsurfaceRadius);
    VALIDATE_OFFSET(subsurfaceScale);
    VALIDATE_OFFSET(subsurfaceAnisotropy);
    VALIDATE_OFFSET(sheen);
    VALIDATE_OFFSET(sheenColor);
    VALIDATE_OFFSET(sheenRoughness);
    VALIDATE_OFFSET(coat);
    VALIDATE_OFFSET(coatColor);
    VALIDATE_OFFSET(coatRoughness);
    VALIDATE_OFFSET(coatAnisotropy);
    VALIDATE_OFFSET(coatRotation);
    VALIDATE_OFFSET(coatIOR);
    VALIDATE_OFFSET(coatAffectColor);
    VALIDATE_OFFSET(coatAffectRoughness);
    VALIDATE_OFFSET(opacity);
    VALIDATE_OFFSET(thinWalled);
    VALIDATE_OFFSET(hasBaseColorTex);
    VALIDATE_OFFSET(baseColorTexRotation);
    VALIDATE_OFFSET(hasSpecularRoughnessTex);
    VALIDATE_OFFSET(specularRoughnessTexOffset);
    VALIDATE_OFFSET(hasOpacityTex);
    VALIDATE_OFFSET(opacityTexPivot);
    VALIDATE_OFFSET(hasNormalTex);
    VALIDATE_OFFSET(normalTexScale);
    VALIDATE_OFFSET(normalTexRotation);

    // Validate structure size
    if (cbDesc.Size != uniformBuffer.size())
    {
        AU_ERROR("%s struct sizes differ: %d!=%d", cbDesc.Name, cbDesc.Size, uniformBuffer.size());
        isValid = false;
    }

    return isValid;
}

PTMaterial::PTMaterial(PTRenderer* pRenderer, MaterialShaderPtr pShader, shared_ptr<MaterialDefinition> pDef) :
    MaterialBase(pShader, pDef), _pRenderer(pRenderer)
{
}

bool PTMaterial::update()
{
    // Do nothing if the material is not dirty.
    if (!_bIsDirty)
    {
        return false;
    }

    // Run the type-specific update function on this material.
    definition()->updateFunction()(*this);

    // Create a transfer buffer for the material data if it doesn't already exist.
    if (!_constantBuffer.size)
    {
        _constantBuffer = _pRenderer->createTransferBuffer(uniformBuffer().size(),
            std::to_string(uint64_t(this)) + "MaterialBuffer");
    }

    // Copy the data to the transfer buffer.
    void* pMappedData = _constantBuffer.map(uniformBuffer().size());
    ::memcpy_s(pMappedData, uniformBuffer().size(), uniformBuffer().data(), uniformBuffer().size());
    _constantBuffer.unmap();

    _bIsDirty = false;

    return true;
}

size_t PTMaterial::computeSamplerHash() const
{
    PTSamplerPtr pSampler;

    // Compute hash for base color image sampler.
    pSampler   = dynamic_pointer_cast<PTSampler>(_values.asSampler("base_color_image_sampler"));
    size_t res = pSampler ? pSampler->hash() : _pRenderer->defaultSampler()->hash();

    // Combine with hash for opacity sampler.
    pSampler = dynamic_pointer_cast<PTSampler>(_values.asSampler("opacity_image_sampler"));
    Foundation::hashCombine(
        res, pSampler ? pSampler->hash() : _pRenderer->defaultSampler()->hash());

    return res;
}

void PTMaterial::createSamplerDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const
{
    PTSamplerPtr pSampler;

    // Create a sampler descriptor on the descriptor heap for the base color sampler, if any.
    pSampler = dynamic_pointer_cast<PTSampler>(_values.asSampler("base_color_image_sampler"));
    PTSampler::createDescriptor(
        *_pRenderer, pSampler ? pSampler.get() : _pRenderer->defaultSampler().get(), handle);

    // Increment the heap location pointed to by the handle past the base color image sampler
    // descriptor.
    handle.Offset(increment);

    // Create a sampler descriptor on the descriptor heap for the opacity sampler, if any.
    pSampler = dynamic_pointer_cast<PTSampler>(_values.asSampler("opacity_image_sampler"));
    PTSampler::createDescriptor(
        *_pRenderer, pSampler ? pSampler.get() : _pRenderer->defaultSampler().get(), handle);

    // Increment the heap location pointed to by the handle past the opacity image sampler
    // descriptor.
    handle.Offset(increment);
}

void PTMaterial::createDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle, UINT increment) const
{
    // Create a SRV (descriptor) on the descriptor heap for the base color image, if any.
    PTImagePtr pImage = dynamic_pointer_cast<PTImage>(_values.asImage("base_color_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the base color image SRV.
    handle.Offset(increment);

    // Create a SRV (descriptor) on the descriptor heap for the specular roughness image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("specular_roughness_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the specular roughness image SRV.
    handle.Offset(increment);

    // Create a SRV (descriptor) on the descriptor heap for the normal image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("normal_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the normal image SRV.
    handle.Offset(increment);

    // Create a SRV (descriptor) on the descriptor heap for the normal image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("opacity_image"));
    PTImage::createSRV(*_pRenderer, pImage.get(), handle);

    // Increment the heap location pointed to by the handle past the opacity image SRV.
    handle.Offset(increment);
}

END_AURORA
