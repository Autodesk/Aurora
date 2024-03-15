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

#include "MaterialShader.h"

BEGIN_AURORA

class MaterialBase;

// The default values for the properties and textures contained in a material.
struct MaterialDefaultValues
{
    MaterialDefaultValues() {}
    MaterialDefaultValues(const UniformBufferDefinition& propertyDefs,
        const vector<PropertyValue>& defaultProps, const vector<TextureDefinition>& defaultTxt);

    // The names of the textures defined for this material.
    vector<TextureIdentifier> textureNames;

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
    MaterialDefinition(const MaterialShaderSource& source, const MaterialDefaultValues& defaults,
        function<void(MaterialBase&)> updateFunc, bool isAlwaysOpaque) :
        _source(source),
        _defaults(defaults),
        _updateFunc(updateFunc),
        _isAlwaysOpaque(isAlwaysOpaque)
    {
    }
    MaterialDefinition() {}

    // Create a shader definition from this material definition.
    // A shader definition is a subset of the material definition.
    // Material definitions with the same property definitions, but different default values, will
    // produce the same shader definition.
    void getShaderDefinition(MaterialShaderDefinition& defOut) const
    {
        defOut.source              = _source;
        defOut.textureNames        = defaults().textureNames;
        defOut.propertyDefinitions = defaults().propertyDefinitions;
        defOut.isAlwaysOpaque      = _isAlwaysOpaque;
    }

    // Gets the default values.
    const MaterialDefaultValues& defaults() const { return _defaults; }

    // Gets the source code.
    const MaterialShaderSource& source() const { return _source; }

    // Gets the update the function, that is invoked when the material is updated.
    function<void(MaterialBase&)> updateFunction() const { return _updateFunc; }

    // Returns whether the material is always opaque, regardless of the property values.
    bool isAlwaysOpaque() const { return _isAlwaysOpaque; }

private:
    // The source code (and unique shader ID) for this material.
    MaterialShaderSource _source;

    // The default values for the material.
    MaterialDefaultValues _defaults;

    // The update function, called when material is changed.
    function<void(MaterialBase&)> _updateFunc;

    // Whether this material is guaranteed to be always opaque, regardless of the property values.
    bool _isAlwaysOpaque;
};

// Shared point to material definition.
using MaterialDefinitionPtr = shared_ptr<MaterialDefinition>;

END_AURORA
