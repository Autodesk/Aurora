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

class TextureProperties
{
public:
    struct TextureProperty
    {
        TextureIdentifier name;
        IImagePtr image;
        ISamplerPtr sampler;
    };

    TextureProperties(const vector<TextureIdentifier>& names)
    {
        for (int i = 0; i < names.size(); i++)
        {
            auto& name                     = names[i];
            _textureNameLookup[name.image] = int(_properties.size());
            if (name.hasSampler())
                _samplerNameLookup[name.sampler] = int(_properties.size());
            _properties.push_back({ name, nullptr, nullptr });
        }
    }
    const TextureProperty& get(int n) const { return _properties[n]; }
    int findTexture(const string& name) const
    {
        auto iter = _textureNameLookup.find(name);
        if (iter == _textureNameLookup.end())
            return -1;
        return iter->second;
    }
    int findSampler(const string& name) const
    {
        auto iter = _samplerNameLookup.find(name);
        if (iter == _samplerNameLookup.end())
            return -1;
        return iter->second;
    }

    void setTexture(const string& name, IImagePtr pImage)
    {
        int idx = findTexture(name);
        AU_ASSERT(idx >= 0, "Invalid index");
        _properties[idx].image = pImage;
    }
    void setSampler(const string& name, ISamplerPtr pSampler)
    {
        int idx = findSampler(name);
        AU_ASSERT(idx >= 0, "Invalid index");
        _properties[idx].sampler = pSampler;
    }
    IImagePtr getTexture(const string& name) const
    {
        int idx = findTexture(name);
        AU_ASSERT(idx >= 0, "Invalid index");
        return _properties[idx].image;
    }
    ISamplerPtr getSampler(const string& name) const
    {
        int idx = findSampler(name);
        AU_ASSERT(idx >= 0, "Invalid index");
        return _properties[idx].sampler;
    }

    size_t count() const { return _properties.size(); }

private:
    vector<TextureProperty> _properties;
    map<string, int> _textureNameLookup;
    map<string, int> _samplerNameLookup;
};

// A base class for implementations of IMaterial.
class MaterialBase : public IMaterial, public IValues
{
public:
    /*** Lifetime Management ***/

    MaterialBase(const string& name, MaterialShaderPtr pShader, MaterialDefinitionPtr pDef);

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
    void setImage(const string& name, const IImagePtr& value) override
    {
        _textures.setTexture(name, value);
        _bIsDirty = true;
    }

    // Override default setter.
    void setSampler(const string& name, const ISamplerPtr& value) override
    {
        _textures.setSampler(name, value);
        _bIsDirty = true;
    }

    void setString(const string& /* name*/, const string& /* value*/) override
    {
        AU_FAIL("Cannot set arbitrary string on material.");
    }
    IValues::Type type(const std::string& name) override
    {
        if (_uniformBuffer.contains(name))
        {
            switch (_uniformBuffer.getType(name))
            {
            case PropertyValue::Type::Bool:
                return IValues ::Type::Boolean;
            case PropertyValue::Type::Int:
                return IValues ::Type::Int;
            case PropertyValue::Type::Float:
                return IValues ::Type::Float;
            case PropertyValue::Type::Float2:
                return IValues ::Type::Float2;
            case PropertyValue::Type::Float3:
                return IValues ::Type::Float3;
            default:
                return IValues::Type::Undefined;
            }
        }
        else
        {
            if (_textures.findSampler(name) >= 0)
                return IValues::Type::Sampler;
            if (_textures.findTexture(name) >= 0)
                return IValues::Type::Image;
        }
        return IValues::Type::Undefined;
    }

    // Override default clear function.
    void clearValue(const string& name) override
    {
        if (_uniformBuffer.contains(name))
        {
            _uniformBuffer.reset(name);
        }
        else
        {
            int idx;
            idx = _textures.findTexture(name);
            if (idx >= 0)
            {
                _textures.setTexture(name, nullptr);
            }
            else
            {
                idx = _textures.findSampler(name);
                if (idx >= 0)
                    _textures.setSampler(name, nullptr);
            }
        }
        _bIsDirty = true;
        return;
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
    static vector<TextureIdentifier> StandardSurfaceTextures;
    static MaterialDefaultValues StandardSurfaceDefaults;

    // Update function for built-in materials.
    static void updateBuiltInMaterial(MaterialBase& mtl);

    shared_ptr<MaterialDefinition> definition() { return _pDef; }
    const shared_ptr<const MaterialDefinition> definition() const { return _pDef; }

    const TextureProperties& textures() { return _textures; }

protected:
    bool _isOpaque = true;
    bool _bIsDirty = true;

private:
    MaterialDefinitionPtr _pDef;
    MaterialShaderPtr _pShader;
    UniformBuffer _uniformBuffer;
    TextureProperties _textures;
    string _name;
};

END_AURORA
