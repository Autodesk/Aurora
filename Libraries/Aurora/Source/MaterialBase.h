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

#include "MaterialDefinition.h"
#include "Properties.h"
#include "UniformBuffer.h"

BEGIN_AURORA

// A base class for implementations of IMaterial.
class MaterialBase : public IMaterial, public FixedValues
{
public:
    /*** Lifetime Management ***/

    MaterialBase(MaterialShaderPtr pShader, MaterialDefinitionPtr pDef);

    /*** IMaterial Functions ***/

    // Gets the material shader for this material.
    MaterialShaderPtr shader() const { return _pShader; }

    IValues& values() override { return *this; }

    // Override default setter.
    void setBoolean(const string& name, bool value) override
    {
        _uniformBuffer.set(name, value);
        _bIsDirty = true;
    }

    // Override default setter.
    void setInt(const string& name, int value) override
    {
        _uniformBuffer.set(name, value);
        _bIsDirty = true;
    }

    // Override default setter.
    void setFloat(const string& name, float value) override
    {
        _uniformBuffer.set(name, value);
        _bIsDirty = true;
    }

    // Override default setter.
    void setFloat2(const string& name, const float* value) override
    {
        _uniformBuffer.set(name, glm::make_vec2(value));
        _bIsDirty = true;
    }

    // Override default setter.
    void setFloat3(const string& name, const float* value) override
    {
        _uniformBuffer.set(name, glm::make_vec3(value));
        _bIsDirty = true;
    }

    // Override default setter.
    void setMatrix(const string& name, const float* value) override
    {
        _uniformBuffer.set(name, glm::make_mat4(value));
        _bIsDirty = true;
    }

    // Override default setter.
    // TODO: Correctly map arbritrary texture names.
    void setImage(const string& name, const IImagePtr& value) override
    {
        FixedValues::setImage(name, value);
    }

    // Override default setter.
    // TODO: Correctly map arbritrary texture names.
    void setSampler(const string& name, const ISamplerPtr& value) override
    {
        FixedValues::setSampler(name, value);
    }

    // Override default clear function.
    void clearValue(const string& name) override
    {
        if (_uniformBuffer.contains(name))
        {
            _uniformBuffer.reset(name);
            _bIsDirty = true;
            return;
        }
        FixedValues::clearValue(name);
    }

    // Is this material opaque?
    bool isOpaque() const { return _isOpaque; }

    // Set the opaque flag.
    void setIsOpaque(bool val) { _isOpaque = val; }

    // Get the uniform buffer for this material.
    UniformBuffer& uniformBuffer() { return _uniformBuffer; }
    const UniformBuffer& uniformBuffer() const { return _uniformBuffer; }

    // Hard-coded Standard Surface properties, textures and defaults used by built-in materials.
    static UniformBufferDefinition StandardSurfaceUniforms;
    static vector<string> StandardSurfaceTextures;
    static MaterialDefaultValues StandardSurfaceDefaults;

    // Update function for built-in materials.
    static void updateBuiltInMaterial(MaterialBase& mtl);

    shared_ptr<MaterialDefinition> definition() { return _pDef; }
    const shared_ptr<MaterialDefinition> definition() const { return _pDef; }

protected:
    bool _isOpaque = true;

private:
    MaterialDefinitionPtr _pDef;
    MaterialShaderPtr _pShader;
    UniformBuffer _uniformBuffer;
};

END_AURORA
