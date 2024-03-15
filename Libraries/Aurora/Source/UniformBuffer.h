//
// Copyright 2023 by Autodesk, Inc.  All rights reserved.
//
// This computer source code and related instructions and comments
// are the unpublished confidential and proprietary information of
// Autodesk, Inc. and are protected under applicable copyright and
// trade secret law.  They may not be disclosed to, copied or used
// by any third party without the prior written consent of Autodesk, Inc.
//
#pragma once

#include "Properties.h"

BEGIN_AURORA

// Definition of a uniform buffer property.
struct UniformBufferPropertyDefinition
{

    UniformBufferPropertyDefinition(
        const string& nm, const string& varName, PropertyValue::Type type) :
        name(nm), variableName(varName), type(type)
    {
    }

    // Human readable name of property.
    string name;
    // C-like variable name for property.
    string variableName;
    // The type of the property.
    PropertyValue::Type type = PropertyValue::Type::Undefined;
};

// A uniform buffer definition is just an array of property definitions objects.
// TODO: Add a UniformBufferDefinition class that can contain a lot of the stuff (e.g. the fields)
// currently in UniformBuffer.
using UniformBufferDefinition = vector<UniformBufferPropertyDefinition>;

// Texture indentifier, containing image name (e.g. "base_color_image") and optional sampler name
// (e.g. "base_color_image_sampler").
struct TextureIdentifier
{
    string image;
    string sampler;
    TextureIdentifier(const char* pImageName = nullptr) :
        image(pImageName ? pImageName : ""), sampler("")
    {
    }
    TextureIdentifier(const string& imageName, const string& samplerName = "") :
        image(imageName), sampler(samplerName)
    {
    }
    bool hasSampler() const { return !sampler.empty(); }
};

// Definition of a texture.
struct TextureDefinition
{
    // Texture name.
    TextureIdentifier name;
    // Is true, the texture is sRGB that should be converted to linear color space on the GPU.
    bool linearize = true;
    // Default filename for texture.
    string defaultFilename = "";
    // Default U address mode for texture (can have values: periodic, clamp, or mirror)
    string addressModeU = "";
    // Default V address mode for texture (can have values: periodic, clamp, or mirror)
    string addressModeV = "";
};

// Class representing generic uniform buffer, and a collection of named scalar and vector properties
// packed into a contiguous chunk of memory.  Used to represent a material's properties in a
// data-driven manner.
class UniformBuffer
{
public:
    // Constructor.
    UniformBuffer(const UniformBufferDefinition& definition, const vector<PropertyValue>& defaults);

    // Sets a named property in the buffer.
    template <typename ValType>
    void set(const string& name, const ValType& val)
    {
        PropertyValue propVal = val;
        set(name, propVal);
    }

    // Gets a named property from the buffer.
    template <typename ValType>
    ValType get(const string& name) const
    {
        auto pField = getField(name);
        ValType res = ValType();
        if (!pField)
        {
            AU_ERROR("No property named %s in uniform buffer.", name.c_str());
            return res;
        }
        PropertyValue::Type type = _definition[pField->index].type;
        AU_ASSERT(getSizeOfType(type) == sizeof(ValType), "Type mismatch.");
        res = *(ValType*)&_data[pField->bufferIndex];

        return res;
    }

    // Resets a property to its default value.
    void reset(const string& name);

    // Gets the size of the buffer in bytes.
    size_t size() const { return _data.size() * sizeof(_data[0]); }

    // Gets the contents of the buffer.
    void* data() { return _data.data(); }
    const void* data() const { return _data.data(); }

    // Generates a HLSL struct from this buffer.
    string generateHLSLStruct() const;

    // Generates a HLSL struct and accessors from this buffer.
    string generateHLSLStructAndAccessors(const string& structName, const string& prefix) const;

    // Generates the HLSL accessor functions for this buffer, with a ByteAddressBuffer being used
    // for storage. Each function name is prefixed with the specified string, e.g. using a
    // "Material42_" prefix might produce the following string:
    //
    // "float Material42_specularRoughness(ByteAddressBuffer buf) { return asfloat(buf.Load(44)); }"
    string generateByteAddressBufferAccessors(const string& prefix) const;

    // Gets the offset (in bytes) of a property in the buffer
    size_t getOffset(const string& propertyName) const;

    // Gets the index of this property in buffer (or -1 if not found.)
    size_t getIndex(const string& propertyName) const;

    // Gets the offset (in bytes) of a variable name in the buffer
    size_t getOffsetForVariable(const string& varName) const;

    // Gets the type of a property.
    PropertyValue::Type getType(const string& propertyName) const;

    // Gets the variable name of a property.
    const string& getVariableName(const string& propertyName) const;

    // Returns whether the uniform buffer contains the named property.
    bool contains(const string& propertyName) const { return getField(propertyName) != nullptr; }

private:
    // A field within the buffer, can be property or empty padding field (in which case index is -1)
    struct Field
    {
        // Index within data buffer.
        size_t bufferIndex = 0;
        // Type of property.
        PropertyValue::Type type = PropertyValue::Type::Undefined;
        // Index to definition in _definition (-1 if padding)
        int index = 0;
    };
    const UniformBufferPropertyDefinition* getPropertyDef(const string& propertyName) const;
    const Field* getField(const string& propertyName) const;

    void set(const string& name, const PropertyValue& val);

    template <typename ValType>
    size_t copyToBuffer(const ValType& val, size_t bufferIndex)
    {
        size_t numWords = sizeof(ValType) / sizeof(_data[0]);
        if (_data.size() < bufferIndex + numWords)
        {
            _data.resize(bufferIndex + numWords);
        }
        ValType* pDstData = (ValType*)&_data[bufferIndex];
        *pDstData         = val;
        return bufferIndex + numWords;
    }

    static size_t getSizeOfType(PropertyValue::Type type);
    static string getGLSLStringFromType(PropertyValue::Type type);
    static string getHLSLStringFromType(PropertyValue::Type type);
    static size_t getAlignment(PropertyValue::Type type);
    size_t copyToBuffer(const PropertyValue& val, size_t bufferIndex);
    const Field* findField(const string& name);
    vector<Field> _fields;
    vector<uint32_t> _data;
    map<string, size_t> _fieldMap;
    map<string, size_t> _fieldVariableMap;
    const UniformBufferDefinition& _definition;
    const vector<PropertyValue>& _defaults;
};

END_AURORA
