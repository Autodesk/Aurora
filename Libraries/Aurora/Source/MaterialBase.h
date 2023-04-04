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

#include "Properties.h"
#include "UniformBuffer.h"

BEGIN_AURORA

class MaterialBase;

// Representation of the shader source for a material type.  Contains source code itself and a
// unique name string.
struct MaterialTypeSource
{
    MaterialTypeSource(const string& typeName = "", const string& setupSource = "",
        const string& bsdfSource = "") :
        name(typeName), setup(setupSource), bsdf(bsdfSource)
    {
    }

    // Compare the source itself.
    bool compareSource(const MaterialTypeSource& other) const
    {
        return (setup.compare(other.setup) == 0 && bsdf.compare(other.bsdf) == 0);
    }

    // Compare the name.
    bool compare(const MaterialTypeSource& other) const { return (name.compare(other.name) == 0); }

    // Reset the contents to empty strings.
    void reset()
    {
        name  = "";
        setup = "";
        bsdf  = "";
    }

    // Is there actually source associated with this material type?
    bool empty() const { return name.empty(); }

    // Unique name (assigning different source the same name will result in errors).
    string name;

    // Shader source for material setup.
    string setup;

    // Optional shader source for bsdf.
    string bsdf;
};

// The default values for the properties and textures contained in a material.
struct MaterialDefaultValues
{
    MaterialDefaultValues() {}
    MaterialDefaultValues(const UniformBufferDefinition& propertyDefs,
        const vector<PropertyValue>& defaultProps, const vector<TextureDefinition>& defaultTxt) :
        propertyDefinitions(propertyDefs), properties(defaultProps), textures(defaultTxt)
    {
        AU_ASSERT(defaultProps.size() == propertyDefs.size(),
            "Default properties do not match definition");
        for (int i = 0; i < defaultTxt.size(); i++)
        {
            textureNames.push_back(defaultTxt[i].name);
        }
    }

    // The names of the textures defined for this material.
    vector<string> textureNames;

    // The definitions of the properties defined for this material.
    UniformBufferDefinition propertyDefinitions;

    // Default values for properties, must match order and size of propertyDefinitions.
    vector<PropertyValue> properties;

    // Default values (include texture filename and sampler properties) for textures, must match
    //  match order and size of textureNames.
    vector<TextureDefinition> textures;
};

// The definition of a material. All the data need to create a material: the shader source code, a
// set of default values, and an update callback function.
class MaterialDefinition
{
public:
    MaterialDefinition(const MaterialTypeSource& source, const MaterialDefaultValues& defaults,
        function<void(MaterialBase&)> updateFunc) :
        _source(source), _defaults(defaults), _updateFunc(updateFunc)
    {
    }
    MaterialDefinition() {}

    // Get the material source code.
    const MaterialTypeSource& source() const { return _source; }

    // Get the default value.
    const MaterialDefaultValues& defaults() const { return _defaults; }

    // Get the update the function, that is invoked when the material is updated.
    const function<void(MaterialBase&)>& updateFunction() const { return _updateFunc; }

private:
    // A shared pointer to the material type, shared_ptr so material type will kept alive for
    // lifetime of this object.
    MaterialTypeSource _source;
    // The default values for the material.
    MaterialDefaultValues _defaults;
    // The update function, called when material is changed.
    function<void(MaterialBase&)> _updateFunc;
};

// A base class for implementations of IMaterial.
class MaterialBase : public IMaterial, public FixedValues
{
public:
    /*** Lifetime Management ***/

    MaterialBase(shared_ptr<MaterialDefinition> pDef);

    /*** IMaterial Functions ***/

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

protected:
    bool _isOpaque = true;

private:
    shared_ptr<MaterialDefinition> _pDef;
    UniformBuffer _uniformBuffer;
};

END_AURORA
