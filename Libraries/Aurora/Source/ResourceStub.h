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

#include "Properties.h"
#include <ResourceTracker.h>

BEGIN_AURORA

class ResourceStub;

// Define resource stub pointer and map type.
using ResourceStubPtr = shared_ptr<ResourceStub>;
using ResourceMap     = map<Path, ResourceStubPtr>;
// TODO: Should be tbb::concurrent_hash_map<Path,ResourceStubPtr>;

// Function applicator types, used to apply resource stub properties to actual renderer resource
// object. These should be defined by the resource sub-class to actually implement the renderer
// resource.
/// Apply a path property from resource stub to actual resource.
using ApplyPathPropertyFunction = function<void(string, Path)>;
/// Apply a path array property from resource stub to actual resource.
using ApplyPathArrayPropertyFunction = function<void(string, vector<Path>)>;
/// Apply a string property from resource stub to actual resource.
using ApplyStringPropertyFunction = function<void(string, string)>;
/// Apply a bool property from resource stub to actual resource.
using ApplyBoolPropertyFunction = function<void(string, bool)>;
/// Apply a float property from resource stub to actual resource.
using ApplyFloatPropertyFunction = function<void(string, float)>;
/// Apply a int property from resource stub to actual resource.
using ApplyIntPropertyFunction = function<void(string, int)>;
/// Apply a vec2 property from resource stub to actual resource.
using ApplyVec2PropertyFunction = function<void(string, glm::vec2)>;
/// Apply a vec3 property from resource stub to actual resource.
using ApplyVec3PropertyFunction = function<void(string, glm::vec3)>;
/// Apply a vec4 property from resource stub to actual resource.
using ApplyVec4PropertyFunction = function<void(string, glm::vec4)>;
/// Apply a matrix4 property from resource stub to actual resource.
using ApplyMat4PropertyFunction = function<void(string, glm::mat4)>;
/// Clear a property that has been cleared in resource stub in the actual resource.
using ApplyClearedPropertyFunction = function<void(string)>;

class ResourceStub;

/// A resource stub is the CPU representation of a renderer resource that remains permanently in
/// memory. The resource stub holds all the properties associated with the resource, and a set of
/// references to the resource stubs for any references (e.g. a Material resource stub will hold a
/// reference to the resource stub of a referenced image.).
///
/// A resource stub becomes active when it is added to the scene, or something referencing it
/// becomes active (e.g. an image becomes active when it is added to a material, that is set on an
/// instance in the scene.)  When a resource stub is activated the actual renderer resource
/// associated with it is created. A resource stub becomes inactive when it is no longer refenced by
/// something in the scene, and its renderer resource is destroyed.
///
/// It is also possible to manually activate a resource stub before it is added to the scene, by
/// incrementing the permanent reference count of the stub. Any stub with a permanent reference
/// count of greater than one is always active, regardless of whether it is added to the scene or
/// not.
///
/// The stub contains all the required data to create the actual resource when the stub becomes
/// active but none of the large data buffers, which should be retreived via callback function.
///
/// This class should be sub-classed by the various resource types (Material, Image, Geometry,
/// Instance, etc.) to implement the specifics of creating and destroying that resource. The sub
/// class should also set up the applicator funcitons that define how the stubs properties are
/// applied to the actual renderer resource.
class ResourceStub
{
public:
    /// \param path The unique path for this resource.
    /// \param container The parent container for this resource and any resources it references.
    ResourceStub(
        const Path& path, const ResourceMap& container, const ResourceTracker& tracker = {}) :
        _path(path), _container(container), _tracker(tracker)
    {
    }
    ResourceStub(const ResourceStub& s) = delete;

    /// Creates the actual renderer resource for this resource stub when activated.  Should be
    /// overriden by specific resource sub-class.
    virtual void createResource() = 0;

    /// Destroys the actual renderer resource for this resource stub when deactivated.  Should be
    /// overriden by specific resource sub-class.
    virtual void destroyResource() = 0;

    virtual const ResourceType& type() = 0;

    /// Set some or all of the properties of this resource.
    ///
    /// Modifiying path properties will also result adding a reference from this resource stub to
    /// the one referenced by the path.
    ///
    /// If the resource stub is currently active this will also apply to the properties to the
    /// actual renderer resource.
    void setProperties(const Properties& props);

    /// Is this resource stub currently active?
    bool isActive() { return _permanentReferenceCount > 0 || _activeReferenceCount > 0; }

    /// Increment the permanent reference count of this stub.  If the permanent reference count is
    /// currently zero, this will cause the resource stub to be activated.
    bool incrementPermanentRefCount();

    /// Decrement the permanent reference count of this stub.  If the permanent reference count is
    /// currently one, this will cause the resource stub to be activated.
    bool decrementPermanentRefCount();

    /// Get a pointer the resource stub referenced by the provided path property name.
    ///
    /// \return Shared pointer to the referenced resource stub, or null pointer if not found.
    ResourceStubPtr getReference(const string& pathPropertyName);

    /// Get a pointer the specific type of resource stub referenced by the provided path property
    /// name.
    ///
    /// \param name The property name associated with the reference.
    /// \return Shared pointer to the referenced resource stub, or null pointer if not found or is
    /// not of type ResourceClass.
    template <typename ResourceClass>
    shared_ptr<ResourceClass> getReference(const string& name)
    {
        ResourceStubPtr pBase = getReference(name);
        if (!pBase)
            return nullptr;
        return dynamic_pointer_cast<ResourceClass>(pBase);
    }

    /// Get the resource from the resource stub referenced by the
    /// provided path property name.
    ///
    /// \param name The property name associated with the reference.
    /// \return Shared pointer to the referenced resource object, or null pointer if not found or is
    /// not of type ResourceClass, or the referenced resource is not active.
    template <typename ResourceClass, typename Resource>
    shared_ptr<Resource> getReferenceResource(const string& name)
    {
        auto pRes = getReference<ResourceClass>(name);
        return pRes ? pRes->resource() : nullptr;
    }

    /// Get the resource from the resource stub referenced by the
    /// provided path property name.
    ///
    /// \param name The property name associated with the reference.
    /// \return Shared pointer to the referenced resource object, or null pointer if not found or is
    /// not of type ResourceClass, or the referenced resource is not active.
    template <typename ResourceClass, typename Resource>
    vector<shared_ptr<Resource>> getReferenceResources(const string& name)
    {
        if (!_properties[name].hasValue())
            return {};

        vector<shared_ptr<Resource>> res;
        const auto& arrayProp = _properties[name].asStrings();
        for (size_t i = 0; i < arrayProp.size(); i++)
        {
            string propName = getIndexedPropertyName(name, static_cast<int>(i));
            res.push_back(getReferenceResource<ResourceClass, Resource>(propName));
        }
        return res;
    }

    /// String used to define default property applicator function.
    /// Any applicator function for this property name is applied to any property of a given type
    /// that has no specific applicator function applied.
    static const string kDefaultPropName;

    /// Get the unique path.
    const Path& path() const { return _path; }

protected:
    /// Invalidate this resource stub (will trigger resource destruction and recreation if currently
    /// active).
    void invalidate()
    {
        // TODO: This needs to be implemented or commands like setImageDescriptor, and
        // setMaterialType will have no effect on activated resources.
    }

    /// Shutdown this resource stub.
    /// \note Due to destructor ordering issues, must be called in the dtor of any resource
    /// sub-classes.
    void shutdown();

    /// Initialize resource stub properties (called from sub-class constructor to set default
    /// properties for resource stub.)
    void initializeProperties(const Properties& properties) { _properties = properties; }

    /// Get the properties map.
    Properties& properties() { return _properties; }

    /// Initialize the map of applicator callback functions used to apply path properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializePathApplicators(const map<string, ApplyPathPropertyFunction>& applicators)
    {
        _pathPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply path array properties in
    /// the renderer resource. Should be called by the constructor of any sub-class to define
    /// resource properties.
    void initializePathArrayApplicators(
        const map<string, ApplyPathArrayPropertyFunction>& applicators)
    {
        _pathArrayPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply string properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    ///
    /// \note As path and string properties use identical types, all string properties are assumed
    /// to be paths (and have an associated reference), rather than plain strings, unless a string
    /// applicator function is defined for them.
    void initializeStringApplicators(const map<string, ApplyStringPropertyFunction>& applicators)
    {
        _stringPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply bool properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeBoolApplicators(const map<string, ApplyBoolPropertyFunction>& applicators)
    {
        _boolPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply float properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeFloatApplicators(const map<string, ApplyFloatPropertyFunction>& applicators)
    {
        _floatPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply int properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeIntApplicators(const map<string, ApplyIntPropertyFunction>& applicators)
    {
        _intPropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply vec2 properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeVec2Applicators(const map<string, ApplyVec2PropertyFunction>& applicators)
    {
        _vec2PropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply vec3 properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeVec3Applicators(const map<string, ApplyVec3PropertyFunction>& applicators)
    {
        _vec3PropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply vec4 properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeVec4Applicators(const map<string, ApplyVec4PropertyFunction>& applicators)
    {
        _vec4PropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to apply mat4 properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeMat4Applicators(const map<string, ApplyMat4PropertyFunction>& applicators)
    {
        _mat4PropertyApplicators = applicators;
    }

    /// Initialize the map of applicator callback functions used to clear properties in the
    /// renderer resource. Should be called by the constructor of any sub-class to define resource
    /// properties.
    void initializeClearedApplicators(const map<string, ApplyClearedPropertyFunction>& applicators)
    {
        _clearedPropertyApplicators = applicators;
    }

private:
    void activate();
    void deactivate();
    void setReference(const string& name, const Path& path);
    void applyProperty(const string& name, const PropertyValue& prop);
    bool incrementActiveRefCount();
    bool decrementActiveRefCount();
    void clearReferences();
    string getIndexedPropertyName(string propName, int index)
    {
        return propName + "[" + to_string(index) + "]";
    }

    map<string, ApplyPathPropertyFunction> _pathPropertyApplicators;
    map<string, ApplyPathArrayPropertyFunction> _pathArrayPropertyApplicators;
    map<string, ApplyStringPropertyFunction> _stringPropertyApplicators;
    map<string, ApplyBoolPropertyFunction> _boolPropertyApplicators;
    map<string, ApplyFloatPropertyFunction> _floatPropertyApplicators;
    map<string, ApplyIntPropertyFunction> _intPropertyApplicators;
    map<string, ApplyVec2PropertyFunction> _vec2PropertyApplicators;
    map<string, ApplyVec3PropertyFunction> _vec3PropertyApplicators;
    map<string, ApplyVec4PropertyFunction> _vec4PropertyApplicators;
    map<string, ApplyMat4PropertyFunction> _mat4PropertyApplicators;
    map<string, ApplyClearedPropertyFunction> _clearedPropertyApplicators;

    int _permanentReferenceCount = 0;
    int _activeReferenceCount    = 0;
    Path _path;
    Properties _properties;
    ResourceMap _references;
    const ResourceMap& _container;
    const ResourceTracker _tracker;
    static shared_ptr<ResourceTracker> _spDefaultTracker;
};

END_AURORA
