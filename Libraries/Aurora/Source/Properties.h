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

BEGIN_AURORA

// A value that can take on one of the available types, suitable for use in containers.
// NOTE: This could be replaced with std::variant or boost::variant if available.
class Value
{
public:
    /*** Lifetime Management ***/

    // Constructor: begin with an undefined type (no value).
    Value() : _type(IValues::IValues::Type::Undefined) {}

    // Destructor: clear the value.
    ~Value() { clear(); }

    // Copy constructor: invoke the assignment operator.
    Value(const Value& other) { *this = other; }

    /*** Operators ***/

    // Assignment operator.
    Value& operator=(const Value& other)
    {
        // This value must either be undefined, or match the incoming type, i.e. no changing types.
        AU_ASSERT(_type == IValues::Type::Undefined || other._type == _type,
            "Aurora Value cannot be assigned to a Value of a different type.");

        // Clear the value, and assign the new value, using placement new for non-trivial storage
        // types.
        clear();
        _type = other._type;
        switch (_type)
        {
        case IValues::Type::Boolean:
            _boolean = other._boolean;
            break;
        case IValues::Type::Int:
            _int = other._int;
            break;
        case IValues::Type::Float:
            _float = other._float;
            break;
        case IValues::Type::Float2:
            new (&_float2) vec2(other._float2);
            break;
        case IValues::Type::Float3:
            new (&_float3) vec3(other._float3);
            break;
        case IValues::Type::Matrix:
            new (&_matrix) mat4(other._matrix);
            break;
        case IValues::Type::Image:
            new (&_pImage) IImagePtr(other._pImage);
            break;
        case IValues::Type::Sampler:
            new (&_pSampler) ISamplerPtr(other._pSampler);
            break;
        case IValues::Type::String:
            new (&_string) std::string(other._string);
            break;
        default:
            break;
        }

        return *this;
    }

    /*** Functions ***/

    // Clears the value.
    void clear()
    {
        // Call the destructor for non-trivial types that have a destructor.
        switch (_type)
        {
        case IValues::Type::Image:
            _pImage.~IImagePtr();
            break;
        case IValues::Type::Sampler:
            _pSampler.~ISamplerPtr();
            break;
        default:
            break;
        }

        // Clear the type.
        _type = IValues::Type::Undefined;
    }

    // Gets the type of the value.
    IValues::Type type() const { return _type; }

    // A macro for implementing a constructor (set) and get accessor for the specified type, storage
    // type, and member name.
#define IMPLEMENT_TYPE_FOR_AURORA(TYPE, STORAGE_TYPE, MEMBER)                                      \
    Value(STORAGE_TYPE value) : MEMBER(value), _type(IValues::Type::TYPE) {}                       \
    STORAGE_TYPE as##TYPE() const                                                                  \
    {                                                                                              \
        AU_ASSERT(_type == IValues::Type::TYPE, "Invalid type");                                   \
        return MEMBER;                                                                             \
    }

    // Implement the constructors (set) and get accessors for the supported types.
    IMPLEMENT_TYPE_FOR_AURORA(Boolean, bool, _boolean)
    IMPLEMENT_TYPE_FOR_AURORA(Int, int, _int)
    IMPLEMENT_TYPE_FOR_AURORA(Float, float, _float)
    IMPLEMENT_TYPE_FOR_AURORA(Float2, const vec2&, _float2)
    IMPLEMENT_TYPE_FOR_AURORA(Float3, const vec3&, _float3)
    IMPLEMENT_TYPE_FOR_AURORA(Matrix, const mat4&, _matrix)
    IMPLEMENT_TYPE_FOR_AURORA(Image, const IImagePtr&, _pImage)
    IMPLEMENT_TYPE_FOR_AURORA(Sampler, const ISamplerPtr&, _pSampler)
    IMPLEMENT_TYPE_FOR_AURORA(String, const std::string&, _string)

private:
    /*** Private Variables ***/
    // NOTE: Together these variables and the functions above implement a tagged anonymous union.

    union
    {
        bool _boolean;
        int _int;
        float _float;
        vec2 _float2;
        vec3 _float3;
        mat4 _matrix;
        IImagePtr _pImage;
        ISamplerPtr _pSampler;
        std::string _string;
    };

    IValues::Type _type = IValues::Type::Undefined;
};

// A description of a property, with a name, type, and default value.
class Property
{
public:
    /*** Lifetime Management ***/
    Property() : _type(IValues::Type::Undefined) {}
    Property(const string& name, size_t index, const Value& defaultValue) :
        _name(name), _index(index), _type(defaultValue.type()), _default(defaultValue)
    {
    }

    /*** Functions ***/

    // Gets the index assigned to the property, used for tracking it in a property set.
    size_t index() const { return _index; }

    // Get the type of the property.
    IValues::Type type() const { return _type; }

    // Gets the default value of the property, which must have the same type as the property.
    const Value& defaultValue() const { return _default; }

    // Gets the property's name.
    const string& name() const { return _name; }

private:
    /*** Private Variables ***/

    string _name;
    size_t _index;
    IValues::Type _type;
    Value _default;
};

// A collection of properties, that can be retrieved by name.
class PropertySet : private unordered_map<string, Property>
{
public:
    /*** Functions ***/

    // Adds a property to the property set, with the specified name and default value.
    void add(const string& name, const Value& defaultValue)
    {
        (*this)[name] = Property(name, count(), defaultValue);
    }

    // Gets the property with the specified name, which must exist in the property set.
    const Property& get(CONST string& name) const
    {
        AU_ASSERT(find(name) != end(), "An invalid property name (%s) was specified", name.c_str());
        return at(name);
    }

    // Gets the property with the specified name, which must exist in the property set.
    Property& get(CONST string& name)
    {
        AU_ASSERT(find(name) != end(), "An invalid property name (%s) was specified", name.c_str());
        return at(name);
    }

    // Does set contain this named value?
    bool hasValue(const string& name) const { return find(name) != end(); }

    // Gets the number of properties in the property set.
    size_t count() const { return size(); }

    unordered_map<string, Property>::const_iterator begin() const
    {
        return unordered_map<string, Property>::begin();
    }
    unordered_map<string, Property>::const_iterator end() const
    {
        return unordered_map<string, Property>::end();
    }

private:
    friend class ValueSet;
};
MAKE_AURORA_PTR(PropertySet);

// A collection of values for a property set.
class FixedValueSet : private vector<Value>
{

public:
    /*** Lifetime Management ***/

    // Constructor.
    FixedValueSet(const PropertySetPtr& pPropertySet)
    {
        AU_ASSERT(pPropertySet, "The property set cannot be null.");
        _pPropertySet = pPropertySet;
        resize(pPropertySet->count());
    }

    // Sets a value for the property with the specified name.
    void setValue(const string& name, const Value& value)
    {
        Property& property = _pPropertySet->get(name);
        bool typesMatch    = value.type() == property.type();
        AU_ASSERT(typesMatch, "The property %s exists, but the type does not match.", name.c_str());
        at(property.index()) = value;
    }

    // A macro for implementing a get value accessor for the specified type and storage type.
    // NOTE: When an value does not have a assignment (i.e. has the Undefined type), the get value
    // accessor returns the property default value instead.
#define MAKE_VALUE_GET_FUNC(TYPE, STORAGE_TYPE)                                                    \
    STORAGE_TYPE as##TYPE(const string& name) const                                                \
    {                                                                                              \
        const Property& property = _pPropertySet->get(name);                                       \
        const Value& value       = at(property.index());                                           \
        return value.type() == IValues::Type::Undefined ? property.defaultValue().as##TYPE()       \
                                                        : value.as##TYPE();                        \
    }

    // Implement the get value accessors for the supported types.
    MAKE_VALUE_GET_FUNC(Boolean, bool)
    MAKE_VALUE_GET_FUNC(Int, int)
    MAKE_VALUE_GET_FUNC(Float, float)
    MAKE_VALUE_GET_FUNC(Float2, const vec2&)
    MAKE_VALUE_GET_FUNC(Float3, const vec3&)
    MAKE_VALUE_GET_FUNC(Matrix, const mat4&)
    MAKE_VALUE_GET_FUNC(Image, IImagePtr)
    MAKE_VALUE_GET_FUNC(Sampler, ISamplerPtr)
    MAKE_VALUE_GET_FUNC(String, const std::string&)

    // Clears the value for the property with the specified name.
    void clearValue(const string& name) { at(_pPropertySet->get(name).index()).clear(); }

    unordered_map<string, Property>::const_iterator begin() const { return _pPropertySet->begin(); }
    unordered_map<string, Property>::const_iterator end() const { return _pPropertySet->end(); }

    IValues::Type type(const string& name) { return _pPropertySet->get(name).type(); }

    bool hasValue(const string& name) const { return _pPropertySet->hasValue(name); }

protected:
    /*** Private Variables ***/

    PropertySetPtr _pPropertySet;

    friend ostream& operator<<(ostream& os, const FixedValueSet& values);
};

// A collection of values for a property set.  The property set is initially empty and can be added
// to at any time.
class DynamicValueSet : public unordered_map<string, Value>
{
public:
    /*** Lifetime Management ***/

    // Constructor.
    DynamicValueSet() {}

    // A macro for implementing a get value accessor for the specified type and storage type.
    // NOTE: When an value does not have a assignment (i.e. has the Undefined type), the get value
    // accessor returns the property default value instead.
#define MAKE_VALUE_GET_FUNC_DYNAMIC(TYPE, STORAGE_TYPE)                                            \
    STORAGE_TYPE as##TYPE(const string& name) const                                                \
    {                                                                                              \
        AU_ASSERT(this->find(name) != this->end(), "Invalid property name.");                      \
        const Value& value = at(name);                                                             \
        return value.as##TYPE();                                                                   \
    }

    // Implement the get value accessors for the supported types.
    MAKE_VALUE_GET_FUNC_DYNAMIC(Boolean, bool)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Int, int)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Float, float)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Float2, const vec2&)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Float3, const vec3&)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Matrix, const mat4&)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Image, IImagePtr)
    MAKE_VALUE_GET_FUNC_DYNAMIC(Sampler, ISamplerPtr)
    MAKE_VALUE_GET_FUNC_DYNAMIC(String, const std::string&)

    // Sets a value for the property with the specified name.
    void setValue(const string& name, const Value& value)
    {
        bool typesMatch = (*this)[name].type() == IValues::Type::Undefined ||
            value.type() == (*this)[name].type();
        AU_ASSERT(typesMatch, "The property %s exists, but the type does not match.", name.c_str());
        (*this)[name] = value;
    }

    bool hasValue(const std::string& name) const { return find(name) != end(); }

    // Clears the value for the property with the specified name.
    void clearValue(const string& name) { this->erase(name); }

    // Get the type of the provided property.
    IValues::Type type(const string& name) { return this->at(name).type(); }

protected:
};

inline ostream& writeToStream(ostream& os, const Value& value)
{
    switch (value.type())
    {
    case IValues::Type::Boolean:
        os << (value.asBoolean() ? "true" : "false");
        break;
    case IValues::Type::Float:
        os << value.asFloat();
        break;
    case IValues::Type::Int:
        os << value.asInt();
        break;
    case IValues::Type::Float3:
        os << value.asFloat3().x << ", " << value.asFloat3().y << ", " << value.asFloat3().z;
        break;
    case IValues::Type::Float2:
        os << value.asFloat2().x << ", " << value.asFloat2().y;
        break;
    case IValues::Type::Matrix:
        os << "{ " << value.asMatrix()[0].x << ", " << value.asMatrix()[0].y << ", "
           << value.asMatrix()[0].z << ", " << value.asMatrix()[0].w << " }, ";
        os << "{ " << value.asMatrix()[1].x << ", " << value.asMatrix()[1].y << ", "
           << value.asMatrix()[1].z << ", " << value.asMatrix()[1].w << " }, ";
        os << "{ " << value.asMatrix()[2].x << ", " << value.asMatrix()[2].y << ", "
           << value.asMatrix()[2].z << ", " << value.asMatrix()[2].w << " }, ";
        os << "{ " << value.asMatrix()[3].x << ", " << value.asMatrix()[3].y << ", "
           << value.asMatrix()[3].z << ", " << value.asMatrix()[3].w << " }";
        break;
    case IValues::Type::Image:
        os << "Image:0x" << value.asImage().get();
        break;
    case IValues::Type::Sampler:
        os << "Sampler:0x" << value.asSampler().get();
        break;
    case IValues::Type::String:
        os << value.asString();
        break;
    default:
        os << "Undefined";
        break;
    }

    os << std::endl;
    return os;
}

inline ostream& operator<<(ostream& os, const FixedValueSet& values)
{
    os.precision(5);
    os << std::fixed;

    for (auto it = values._pPropertySet->begin(); it != values._pPropertySet->end(); it++)
    {
        auto prop  = it->second;
        auto value = values.at(prop.index());
        os << prop.name() << " = ";
        if (value.type() == IValues::Type::Undefined)
            value = prop.defaultValue();

        writeToStream(os, value);
    }
    return os;
}

inline ostream& operator<<(ostream& os, const DynamicValueSet& values)
{
    os.precision(5);
    os << std::fixed;

    for (auto it = values.begin(); it != values.end(); it++)
    {
        os << it->first << " = ";
        writeToStream(os, it->second);
    }
    return os;
}

// An implementation for IValues, based on a ValueSet member.
template <typename T>
class ValuesImpl : public IValues
{
public:
    /*** Lifetime Management ***/
    ValuesImpl() {}
    ValuesImpl(const PropertySetPtr& pPropertySet) : _values(pPropertySet) {};

    /*** IValues Functions ***/

    void setBoolean(const string& name, bool value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void setInt(const string& name, int value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void setFloat(const string& name, float value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void setFloat2(const string& name, const float* value) override
    {
        _values.setValue(name, make_vec2(value));
        _bIsDirty = true;
    }

    void setFloat3(const string& name, const float* value) override
    {
        _values.setValue(name, make_vec3(value));
        _bIsDirty = true;
    }

    void setMatrix(const string& name, const float* value) override
    {
        _values.setValue(name, make_mat4(value));
        _bIsDirty = true;
    }

    void setImage(const string& name, const IImagePtr& value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void setSampler(const string& name, const ISamplerPtr& value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void setString(const string& name, const std::string& value) override
    {
        _values.setValue(name, value);
        _bIsDirty = true;
    }

    void clearValue(const string& name) override
    {
        _values.clearValue(name);
        _bIsDirty = true;
    }

    bool hasValue(const string& name) const { return _values.hasValue(name); }

    bool asBoolean(const string& name) const { return _values.asBoolean(name); }

    int asInt(const string& name) const { return _values.asInt(name); }

    float asFloat(const string& name) const { return _values.asFloat(name); }

    const vec3& asFloat3(const string& name) const { return _values.asFloat3(name); }

    const mat4& asMatrix(const string& name) const { return _values.asMatrix(name); }

    IImagePtr asImage(const string& name) const { return _values.asImage(name); }

    ISamplerPtr asSampler(const string& name) const { return _values.asSampler(name); }

    const string& asString(const string& name) const { return _values.asString(name); }

    IValues::Type type(const string& name) override { return _values.type(name); }

protected:
    T _values;
    bool _bIsDirty = true;
};

// An implementation for IValues, based on a ValueSet member.
class FixedValues : public ValuesImpl<FixedValueSet>
{
public:
    FixedValues() = delete;
    FixedValues(const PropertySetPtr& pPropertySet) : ValuesImpl(pPropertySet) {}
};

// An implementation for IValues, based on a ValueSet member.
class DynamicValues : public ValuesImpl<DynamicValueSet>
{
public:
    DynamicValues() : ValuesImpl() {}
};

END_AURORA
