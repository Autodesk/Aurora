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

class ResourceStub;

/// Set of functions to track activation and modification of resources in resource system.
struct ResourceTracker
{
    function<void(const ResourceStub& resource, const Properties& properties)> resourceModified =
        nullptr;
    function<void(const ResourceStub& resource)> resourceActivated   = nullptr;
    function<void(const ResourceStub& resource)> resourceDeactivated = nullptr;
};

template <typename PointerType>
class PointerWrapper
{
public:
    PointerWrapper(PointerType* ptr = nullptr) : m_ptr(ptr) {}

    PointerType& get() { return *m_ptr; }
    const PointerType& get() const { return *m_ptr; }

    operator PointerType&() { return get(); }
    operator const PointerType&() const { return get(); }

    PointerType* ptr() { return m_ptr; }
    const PointerType* ptr() const { return m_ptr; }

private:
    PointerType* m_ptr;
};

// Noitifier, used to provide a list of active GPU resources to the renderer backend.
template <typename ImplementationClass>
class ResourceNotifier
{

public:
    ResourceNotifier() {}

    // Is active resource list empty?
    bool empty() const { return _resourceData.empty(); }

    // Have changes been made to any resources this frame?
    bool modified() const { return _modified; }

    // Get the index for the provided resource implentation within active list.
    // Will return -1 if resource not currently active.
    uint32_t findActiveIndex(shared_ptr<ImplementationClass> pData) const
    {
        auto iter = _indexLookup.find(pData.get());
        if (iter == _indexLookup.end())
            return static_cast<uint32_t>(-1);
        return static_cast<uint32_t>(iter->second);
    }

    // Get the active resource implementations.
    template <typename ImplementationSubClass = ImplementationClass>
    vector<PointerWrapper<ImplementationSubClass>>& resources()
    {
        // Return resource data cast to the requested sub-class.
        // TODO: Can we get rid of dodgy casting?
        return *reinterpret_cast<vector<PointerWrapper<ImplementationSubClass>>*>(&_resourceData);
    }

    // Get number of currently active resources.
    size_t count() const { return _resourceData.size(); }

    void clearModifiedFlag() { _modified = false; }
    void clear()
    {
        _resourceData.clear();
        _indexLookup.clear();
        _modified = true;
    }

    void add(ImplementationClass* pDataPtr)
    {
        _modified = true;
        // Add resource implementation to data list, and add index to lookup.
        _indexLookup[pDataPtr] = _resourceData.size();
        _resourceData.push_back(PointerWrapper(pDataPtr));
    }

private:
    vector<PointerWrapper<ImplementationClass>> _resourceData;
    map<ImplementationClass*, size_t> _indexLookup;
    bool _modified = false;
};

/// Typed tracker that will maintain list of active resource stubs of a given type.
template <typename ResourceClass, typename ImplementationClass>
class TypedResourceTracker
{
public:
    TypedResourceTracker() : _active(true)
    {
        // Setup the tracker callbacks to maintain resource lists.
        _tracker.resourceActivated = [this](const ResourceStub& baseRes) {
            if (!_active)
                return;
            const ResourceClass* pRes = static_cast<const ResourceClass*>(&baseRes);
            _activatedResources.push_back(pRes);
            _currentlyActiveResources.emplace(pRes->path(), pRes);
        };
        _tracker.resourceDeactivated = [this](const ResourceStub& baseRes) {
            if (!_active)
                return;
            const ResourceClass* pRes = static_cast<const ResourceClass*>(&baseRes);
            _deactivatedResources.push_back(pRes);
            _currentlyActiveResources.erase(pRes->path());
        };
        _tracker.resourceModified = [this](const ResourceStub& baseRes, const Properties& props) {
            if (!_active)
                return;
            const ResourceClass* pRes = static_cast<const ResourceClass*>(&baseRes);
            _modifiedResources.push_back(make_pair(pRes, props));
        };
    }

    void shutdown()
    {
        _activatedResources.clear();
        _deactivatedResources.clear();
        _modifiedResources.clear();
        _currentlyActiveResources.clear();
        _active = false;
    }

    // Update the list of active resource implementations for this frame.
    // Will clear the resources for this frame, will do nothing (but will maintain the active
    // resource list) if no changes recorded in the tracker for this frame.
    bool update()
    {
        _modifiedNotifier.clear();

        // If tracker not changed, do nothing.
        // This keeps active resource list from previous frame but clears the modified flag.
        if (!changed())
        {
            _activeNotifier.clearModifiedFlag();
            return false;
        }

        for (auto iter = _modifiedResources.begin(); iter != _modifiedResources.end(); iter++)
        {
            // Get the GPU implementation for the resource stub (sometimes will be null if error in
            // activation)
            const ResourceClass* pRes     = iter->first;
            ImplementationClass* pDataPtr = pRes->resource().get();
            if (pDataPtr)
            {
                _modifiedNotifier.add(pDataPtr);
            }
        }

        // Set the modified flag and clear lists of resources.
        _activeNotifier.clear();

        // Iterate through all the active resources in tracker.
        for (auto iter = _currentlyActiveResources.begin(); iter != _currentlyActiveResources.end();
             iter++)
        {
            // Get the GPU implementation for the resource stub (sometimes will be null if error in
            // activation)
            const ResourceClass* pRes     = iter->second;
            ImplementationClass* pDataPtr = pRes->resource().get();
            if (pDataPtr)
            {
                _activeNotifier.add(pDataPtr);
            }
        }

        // Clear the tracker for this frame.
        clear();
        return true;
    }

    // Get the tracker callbacks.
    const ResourceTracker& tracker() const { return _tracker; }

    // Get the list of resource stubs activated this frame.
    vector<const ResourceClass*> activated() { return _activatedResources; }

    // Get the list of resources stubs deactivated this frame.
    vector<const ResourceClass*> deactivated() { return _deactivatedResources; }

    // Clear the lists for this frame.
    void clear()
    {
        _activatedResources.clear();
        _deactivatedResources.clear();
        _modifiedResources.clear();
    }

    // Have any changes (modifications, activations or deactivations) been made this frame?
    bool changed() const { return _modifiedNotifier.modified() || _activeNotifier.modified(); }

    // Get the currently active set of resource implementations, for this frame.
    ResourceNotifier<ImplementationClass>& active() { return _activeNotifier; }
    const ResourceNotifier<ImplementationClass>& active() const { return _activeNotifier; }

    // Get the set of resources that have been modified this frame.
    ResourceNotifier<ImplementationClass>& modified() { return _modifiedNotifier; }
    const ResourceNotifier<ImplementationClass>& modified() const { return _modifiedNotifier; }

    size_t activeCount() { return _currentlyActiveResources.size(); }

private:
    ResourceTracker _tracker;
    vector<const ResourceClass*> _activatedResources;
    vector<const ResourceClass*> _deactivatedResources;
    vector<pair<const ResourceClass*, Properties>> _modifiedResources;
    map<Path, const ResourceClass*> _currentlyActiveResources;

    ResourceNotifier<ImplementationClass> _activeNotifier;
    ResourceNotifier<ImplementationClass> _modifiedNotifier;
    bool _active;
};

// Hash lookup to build subset of unique objects using provided hash function, building list of
// objects mapped to unique subset.
template <typename ObjectClass, auto HashFunction>
class UniqueHashLookup
{
public:
    // Add object to lookup.
    void add(ObjectClass& obj)
    {
        // Get hash for object and lookup in map.
        size_t hash = HashFunction(obj);
        auto iter   = _indexLookup.find(hash);
        size_t index;

        // If hash does not exist add it.
        if (iter == _indexLookup.end())
        {
            index              = _uniqueObjects.size();
            _indexLookup[hash] = index;
            _uniqueObjects.push_back(&obj);
        }
        else
        {
            // If hash already exists use existing index.
            index = iter->second;
        }
        _indices.push_back(index);
    }

    // Get unique object at provided index.
    vector<PointerWrapper<ObjectClass>>& unique() { return _uniqueObjects; }

    // Get count of objects in list.
    size_t count() { return _indices.size(); }

    // Get Nth object in list.
    ObjectClass& get(size_t n) { return _uniqueObjects[getUniqueIndex(n)]; }

    // Get the unique index for Nth object in list.
    size_t getUniqueIndex(size_t n) { return _indices[n]; }

    // Clear all the contents.
    void clear()
    {
        _indices.clear();
        _uniqueObjects.clear();
        _indexLookup.clear();
    }

private:
    vector<PointerWrapper<ObjectClass>> _uniqueObjects;
    vector<size_t> _indices;
    unordered_map<size_t, size_t> _indexLookup;
};

END_AURORA
