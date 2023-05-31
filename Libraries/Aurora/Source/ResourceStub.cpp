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

#include "ResourceStub.h"

BEGIN_AURORA

const string ResourceStub::kDefaultPropName = "";

void ResourceStub::shutdown()
{
    // Remove all permanent reference counts.
    while (_permanentReferenceCount)
        decrementPermanentRefCount();

    // Clear all references to other resource stubs.
    clearReferences();
}

void ResourceStub::clearReferences()
{
    // Set any reference properties to the empty string.
    Properties props;
    for (auto iter = _references.begin(); iter != _references.end(); iter++)
    {
        props[iter->first] = "";
    }
    setProperties(props);
}

void ResourceStub::setProperties(const Properties& props)
{
    // Count of the number of properties actually applied.
    size_t numApplied = 0;

    // Iterate through all the supplied properties.
    for (auto iter = props.begin(); iter != props.end(); iter++)
    {
        // Get name of property.
        const Path& propName = iter->first;

        // Only apply the property if different from existing value.
        if (_properties[propName] != iter->second)
        {
            // Increment applied count.
            numApplied++;

            // Is this a string property (as in, it does *not* have a string applicator function
            // associated with it)?
            bool isStringProperty =
                _stringPropertyApplicators.find(propName) != _stringPropertyApplicators.end();

            // We assume any string properties that do not have an explicit string applicator
            // function are actually paths not strings.
            if (iter->second.type == PropertyValue::Type::String && !isStringProperty)
            {
                // Setup a reference for this path property.
                const Path& path = iter->second.asString();
                setReference(propName, path);
            }

            // We assume any string array properties are actually paths not strings.
            else if (iter->second.type == PropertyValue::Type::Strings)
            {
                Strings paths = iter->second.asStrings();

                // Set references to all the paths in form "propName[n]"
                for (int i = 0; i < static_cast<int>(paths.size()); i++)
                {
                    string refName = getIndexedPropertyName(propName, i);
                    setReference(refName, paths[i]);
                }

                // Clear any previous references that exist past end of array.
                for (int i = static_cast<int>(paths.size());; i++)
                {
                    string refName = getIndexedPropertyName(propName, i);
                    if (_references.find(refName) == _references.end())
                        break;
                    else
                        setReference(refName, "");
                }
            }
        
            // Apply the property (will apply to the resource itself if we are active.)
            applyProperty(iter->first, iter->second);
        }

    }

    // Call the resource modified callback if the resource is active and some propreties were applied.
    if (isActive() && numApplied && _tracker.resourceModified)
        _tracker.resourceModified(*this, props);
}

void ResourceStub::applyProperty(const string& name, const PropertyValue& prop)
{
    // Set the property in property map.
    _properties[name] = prop;

    // If inactive do nothing else.
    if (!isActive())
        return;

    // Execute string or path applicator functions for this property.
    if (prop.type == PropertyValue::Type::String)
    {
        // Execute explicit string applicator, if any.  This will mean this property is treated as
        // string not path.
        if (_stringPropertyApplicators.find(name) != _stringPropertyApplicators.end())
        {
            _stringPropertyApplicators[name](name, prop.asString());
        }
        // Execute explicit path applicator, if any.
        else if (_pathPropertyApplicators.find(name) != _pathPropertyApplicators.end())
        {
            _pathPropertyApplicators[name](name, prop.asString());
        }
        // Execute default path applicator, if any. (NOTE, execute *before* any string default
        // applicator, even though explicit applicator exectued *after*)
        else if (_pathPropertyApplicators.find(kDefaultPropName) != _pathPropertyApplicators.end())
        {
            _pathPropertyApplicators[kDefaultPropName](name, prop.asString());
        }
        // Execute default string applicator, if any.
        else if (_stringPropertyApplicators.find(kDefaultPropName) !=
            _stringPropertyApplicators.end())
        {
            _stringPropertyApplicators[kDefaultPropName](name, prop.asString());
        }
        // Fail if no string or path applicator found.
        else
            AU_FAIL("Unknown string property %s (and no default string applicator)", name.c_str());
    }
    // Execute path array applicator functions for this property.
    else if (prop.type == PropertyValue::Type::Strings)
    {
        if (_pathArrayPropertyApplicators.find(name) != _pathArrayPropertyApplicators.end())
        {
            _pathArrayPropertyApplicators[name](name, prop.asStrings());
        }
        // Execute default string array applicator, if any.
        else if (_pathArrayPropertyApplicators.find(kDefaultPropName) !=
            _pathArrayPropertyApplicators.end())
        {
            _pathArrayPropertyApplicators[kDefaultPropName](name, prop.asStrings());
        }
        // Fail if no string or path applicator found.
        else
            AU_FAIL("Unknown strings property %s (and no default string applicator)", name.c_str());
    }
    // Execute bool applicator functions for this property.
    else if (prop.type == PropertyValue::Type::Bool)
    {
        // Execute explicit bool applicator, if any.
        if (_boolPropertyApplicators.find(name) != _boolPropertyApplicators.end())
        {
            _boolPropertyApplicators[name](name, prop.asBool());
        }
        // Execute default bool applicator, if any.
        else if (_boolPropertyApplicators.find(kDefaultPropName) != _boolPropertyApplicators.end())
        {
            _boolPropertyApplicators[kDefaultPropName](name, prop.asBool());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown bool propert %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Int)
    {
        // Execute explicit applicator, if any.
        if (_intPropertyApplicators.find(name) != _intPropertyApplicators.end())
        {
            _intPropertyApplicators[name](name, prop.asInt());
        }
        // Execute default applicator, if any.
        else if (_intPropertyApplicators.find(kDefaultPropName) != _intPropertyApplicators.end())
        {
            _intPropertyApplicators[kDefaultPropName](name, prop.asInt());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown int property %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Float)
    {
        // Execute explicit applicator, if any.
        if (_floatPropertyApplicators.find(name) != _floatPropertyApplicators.end())
        {
            _floatPropertyApplicators[name](name, prop.asFloat());
        }
        // Execute default applicator, if any.
        else if (_floatPropertyApplicators.find(kDefaultPropName) !=
            _floatPropertyApplicators.end())
        {
            _floatPropertyApplicators[kDefaultPropName](name, prop.asFloat());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown float property %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Float2)
    {
        // Execute explicit applicator, if any.
        if (_vec2PropertyApplicators.find(name) != _vec2PropertyApplicators.end())
        {
            _vec2PropertyApplicators[name](name, prop.asFloat2());
        }
        // Execute default applicator, if any.
        else if (_vec2PropertyApplicators.find(kDefaultPropName) != _vec2PropertyApplicators.end())
        {
            _vec2PropertyApplicators[kDefaultPropName](name, prop.asFloat2());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown vec2 property %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Float3)
    {
        // Execute explicit applicator, if any.
        if (_vec3PropertyApplicators.find(name) != _vec3PropertyApplicators.end())
        {
            _vec3PropertyApplicators[name](name, prop.asFloat3());
        }
        // Execute default applicator, if any.
        else if (_vec3PropertyApplicators.find(kDefaultPropName) != _vec3PropertyApplicators.end())
        {
            _vec3PropertyApplicators[kDefaultPropName](name, prop.asFloat3());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown vec3 property %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Float4)
    {
        // Execute explicit applicator, if any.
        if (_vec4PropertyApplicators.find(name) != _vec4PropertyApplicators.end())
        {
            _vec4PropertyApplicators[name](name, prop.asFloat4());
        }
        // Execute default applicator, if any.
        else if (_vec4PropertyApplicators.find(kDefaultPropName) != _vec4PropertyApplicators.end())
        {
            _vec4PropertyApplicators[kDefaultPropName](name, prop.asFloat4());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown vec4 property %s (and no default string applicator)", name.c_str());
    }
    else if (prop.type == PropertyValue::Type::Matrix4)
    {
        // Execute explicit applicator, if any.
        if (_mat4PropertyApplicators.find(name) != _mat4PropertyApplicators.end())
        {
            _mat4PropertyApplicators[name](name, prop.asMatrix4());
        }
        // Execute default applicator, if any.
        else if (_mat4PropertyApplicators.find(kDefaultPropName) != _mat4PropertyApplicators.end())
        {
            _mat4PropertyApplicators[kDefaultPropName](name, prop.asMatrix4());
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unknown mat4 property %s (and no default string applicator)", name.c_str());
    }
    // Execute clear applicator (property is cleared if undefined value passed as property)
    else if (prop.type == PropertyValue::Type::Undefined)
    {
        // Execute explicit clear applicator, if any.
        if (_clearedPropertyApplicators.find(name) != _clearedPropertyApplicators.end())
        {
            _clearedPropertyApplicators[name](name);
        }
        // Execute default clear applicator, if any.
        else if (_clearedPropertyApplicators.find(kDefaultPropName) !=
            _clearedPropertyApplicators.end())
        {
            _clearedPropertyApplicators[kDefaultPropName](name);
        }
        // Fail if no applicator found.
        else
            AU_FAIL("Unsupported cleared property %s", name.c_str());
    }
    else
    {
        AU_FAIL("Unknown property type %d for %s", prop.type, name.c_str());
    }
}

ResourceStubPtr ResourceStub::getReference(const string& name)
{
    // Find property name for in references map.
    auto iter = _references.find(name);

    // Return null if none found.
    if (iter == _references.end())
        return nullptr;

    // Return pointer to referenced resource stub.
    return iter->second;
}

void ResourceStub::setReference(const string& name, const Path& path)
{
    ResourceStubPtr pRes = nullptr;

    // If path is not empty (the null case), find the referenced resource in the parent container.
    if (!path.empty())
    {
        auto iter = _container.find(path);
        AU_ASSERT(iter != _container.end(),
            "Failed to set reference in resource %s, path %s not found for property %s",
            _path.c_str(), path.c_str(), name.c_str());

        pRes = iter->second;
    }

    // If the reference hasn't change do nothing.
    if (getReference(name) == pRes)
        return;

    // If this resource is active increment the active ref count on the referenced resource, and
    // decrement the exisiting reference if any.
    if (isActive())
    {
        if (pRes)
            pRes->incrementActiveRefCount();
        if (getReference(name))
            _references[name]->decrementActiveRefCount();
    }

    // Set the reference.
    _references[name] = pRes;
}

bool ResourceStub::incrementActiveRefCount()
{
    // Is the resource stub currently active?
    bool currentlyActive = isActive();

    // Increment ref count.
    _activeReferenceCount++;

    // Activate if it has gone from inactive to active.
    if (currentlyActive != isActive())
    {
        activate();
        return true;
    }
    return false;
}

bool ResourceStub::decrementActiveRefCount()
{
    // Is the resource stub currently active?
    bool currentlyActive = isActive();

    // Can't decrement below zero.
    AU_ASSERT(_activeReferenceCount > 0, "Invalid reference count");

    // Decrement ref count.
    _activeReferenceCount--;

    // Deactivate if it has gone from inactive to active.
    if (currentlyActive != isActive())
    {
        deactivate();
        return true;
    }

    return false;
}

bool ResourceStub::incrementPermanentRefCount()
{
    // Is the resource stub currently active?
    bool currentlyActive = isActive();

    // Decrement ref count.
    _permanentReferenceCount++;

    // Deactivate if it has gone from inactive to active.
    if (currentlyActive != isActive())
    {
        activate();
        return true;
    }
    return false;
}

bool ResourceStub::decrementPermanentRefCount()
{
    // Is the resource stub currently active?
    bool currentlyActive = isActive();

    // Can't decrement below zero.
    AU_ASSERT(_permanentReferenceCount > 0, "Invalid reference count");

    // Decrement ref count.
    _permanentReferenceCount--;

    // Deactivate if it has gone from inactive to active.
    if (currentlyActive != isActive())
    {
        deactivate();
        return true;
    }
    return false;
}

void ResourceStub::activate()
{
    // Iterate through all the referenced resource stubs.
    for (auto iter = _references.begin(); iter != _references.end(); iter++)
    {
        // Increment refenced resource's active reference count.
        if (iter->second)
        {
            iter->second->incrementActiveRefCount();
        }
    }

    // Create the actual renderer resource for this resource stub.
    createResource();

    if (_tracker.resourceActivated)
        _tracker.resourceActivated(*this);

    // Apply the properties to the newly created resource.
    for (auto iter = _properties.begin(); iter != _properties.end(); iter++)
    {
        applyProperty(iter->first, iter->second);
    }

    if (_tracker.resourceModified)
        _tracker.resourceModified(*this, _properties);
}

void ResourceStub::deactivate()
{
    // Iterate through all the referenced resource stubs.
    for (auto iter = _references.begin(); iter != _references.end(); iter++)
    {
        // Decrement refenced resource's active reference count.
        if (iter->second)
        {
            iter->second->decrementActiveRefCount();
        }
    }

    if (_tracker.resourceDeactivated)
        _tracker.resourceDeactivated(*this);

    // Destroy the actual renderer resource for this resource stub.
    destroyResource();
}

END_AURORA
