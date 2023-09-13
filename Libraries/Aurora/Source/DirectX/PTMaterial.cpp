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

PTMaterial::PTMaterial(PTRenderer* pRenderer, const string& name, MaterialShaderPtr pShader,
    shared_ptr<MaterialDefinition> pDef) :
    MaterialBase(name, pShader, pDef), _pRenderer(pRenderer)
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
        _constantBuffer = _pRenderer->createTransferBuffer(
            uniformBuffer().size(), std::to_string(uint64_t(this)) + "MaterialBuffer");
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

void PTMaterial::getTextures(vector<PTImage*>& texturesOut) const
{
    texturesOut.clear();
    // Create a SRV (descriptor) on the descriptor heap for the base color image, if any.
    PTImagePtr pImage = dynamic_pointer_cast<PTImage>(_values.asImage("base_color_image"));
    texturesOut.push_back(pImage.get());

    // Create a SRV (descriptor) on the descriptor heap for the specular roughness image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("specular_roughness_image"));
    texturesOut.push_back(pImage.get());

    // Create a SRV (descriptor) on the descriptor heap for the normal image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("normal_image"));
    texturesOut.push_back(pImage.get());

    // Create a SRV (descriptor) on the descriptor heap for the emission color image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("emission_color_image"));
    texturesOut.push_back(pImage.get());

    // Create a SRV (descriptor) on the descriptor heap for the opacity image, if any.
    pImage = dynamic_pointer_cast<PTImage>(_values.asImage("opacity_image"));
    texturesOut.push_back(pImage.get());
}

END_AURORA
