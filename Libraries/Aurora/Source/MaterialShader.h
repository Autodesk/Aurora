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

#include "Aurora/Foundation/Log.h"
#include "Properties.h"
#include "UniformBuffer.h"

BEGIN_AURORA

class MaterialShader;
class MaterialShaderLibrary;

// Representation of the shader source for a material shader.  Contains source code itself and a
// unique ID string.
struct MaterialShaderSource
{
    MaterialShaderSource(const string& id = "", const string& setupSource = "",
        const string& definitionsSource = "", const string& bsdfSource = "") :
        uniqueId(id), setup(setupSource), definitions(definitionsSource), bsdf(bsdfSource)
    {
    }

    // Compare the source itself.
    bool compareSource(const MaterialShaderSource& other) const
    {
        return (setup.compare(other.setup) == 0 && bsdf.compare(other.bsdf) == 0 &&
            definitions.compare(other.definitions) == 0);
    }

    // Compare the unique IDs.
    bool compare(const MaterialShaderSource& other) const
    {
        return (uniqueId.compare(other.uniqueId) == 0);
    }

    // Reset the contents to empty strings.
    void reset()
    {
        uniqueId                 = "";
        setup                    = "";
        bsdf                     = "";
        definitions              = "";
        setupFunctionDeclaration = "";
    }

    // Is there actually source associated with this material shader?
    bool empty() const { return uniqueId.empty(); }

    // Unique name (assigning different source the same name will result in errors).
    string uniqueId;

    // Shader source for material setup.
    string setup;

    // Optional shader source for bsdf.
    string bsdf;

    // Optional function definitions.
    string definitions;

    // Function declaration for material setup function.
    string setupFunctionDeclaration;
};

// The definitions of a material shader.
// This is a subset of the MaterialDefinition, containing just the source, the property definitions
// (not the values), and the texture names.
struct MaterialShaderDefinition
{
    // Shader source
    MaterialShaderSource source;
    // Definitions of properties for this shader (without the default values.)
    UniformBufferDefinition propertyDefinitions;
    // Names of the textures defined by this shader.
    vector<string> textureNames;
    // Is this shader always opaque regardless of properties.
    bool isAlwaysOpaque;

    bool compare(const MaterialShaderDefinition& other) const
    {
        if (!other.source.compare(source))
            return false;
        if (textureNames.size() != other.textureNames.size())
            return false;
        if (propertyDefinitions.size() != other.propertyDefinitions.size())
            return false;
        if (isAlwaysOpaque != other.isAlwaysOpaque)
            return false;
        return true;
        // TODO also compare contents of arrays.
    }
    bool compareSource(const MaterialShaderDefinition& other) const
    {
        if (!compare(other))
            return false;
        if (!other.source.compareSource(source))
            return false;
        return true;
    }
};

/**
 * \desc Class representing a material shader, which maps directly to a DirectX hit group and a hit
 * shader in the HLSL shader library. Material types are managed by the MaterialShaderLibrary class,
 * and deletes via shared pointer when no longer referenced by any materials.
 */
class MaterialShader
{
    friend class MaterialShaderLibrary;

public:
    /*** Types ***/

    /*** Lifetime Management ***/

    /**
     * NOTE: Do not call ctor directly, instead create via MaterialShaderLibrary.
     *  \param pShaderLibrary The shader library this type is part of.
     *  \param libraryIndex The index for this type's shader code within the library object.
     *  \param def The definition of this shader.
     *  \param entryPoints The entry points for this shader.
     */
    MaterialShader(MaterialShaderLibrary* pShaderLibrary, int libraryIndex,
        const MaterialShaderDefinition& def, const vector<string> entryPoints);
    ~MaterialShader();

    // Gets the index for this type's hit shader within the library object.
    int libraryIndex() const { return _libraryIndex; }

    // Decrement the reference count for the given entry point.
    void incrementRefCount(const string& entryPoint);

    // Decrement the reference count for the given entry point.
    // Will trigger rebuild if the entry point reference count goes from zero to non-zero.
    void decrementRefCount(const string& entryPoint);

    // Get the current reference count for this entry point.
    int refCount(const string& entryPoint) const;

    // Does this entry point exist (true if the entry point has non-zero ref count.)
    bool hasEntryPoint(const string& entryPoint) const { return refCount(entryPoint) > 0; }

    const vector<string>& entryPoints() const { return _entryPointsTypes; }

    // Get the unique ID for the
    const string& id() const { return _def.source.uniqueId; }

    // Get the definition of this shader.
    const MaterialShaderDefinition& definition() const { return _def; }

protected:
    struct EntryPoint
    {
        string name;
        int refCount = 0;
    };

    EntryPoint* getEntryPoint(const string& name)
    {
        auto iter = _entryPointNameLookup.find(name);
        if (iter == _entryPointNameLookup.end())
        {
            return nullptr;
        }
        return &_entryPoints[iter->second];
    }
    const EntryPoint* getEntryPoint(const string& name) const
    {
        auto iter = _entryPointNameLookup.find(name);
        if (iter == _entryPointNameLookup.end())
        {
            return nullptr;
        }
        return &_entryPoints[iter->second];
    }

    // Invalidate this type, by setting its shader library to null pointer.
    // Called by shader library in its destructor to avoid orphaned material types.
    void invalidate() { _pShaderLibrary = nullptr; }

    // Is this type still valid?
    bool isValid() { return _pShaderLibrary != nullptr; }

    vector<EntryPoint> _entryPoints;
    map<string, int> _entryPointNameLookup;
    vector<string> _entryPointsTypes;

    // The shader library this type is part of.
    MaterialShaderLibrary* _pShaderLibrary;

    // The index for this type's shader code within the library.
    int _libraryIndex;

    // Definition of this shader.
    MaterialShaderDefinition _def;
};

// Shared point to material shader.
using MaterialShaderPtr = shared_ptr<MaterialShader>;

// Library of material shaders that manages shader lifetime.
class MaterialShaderLibrary
{
    friend class MaterialShader;

public:
    // State of a shader in this library
    enum CompileState
    {
        Invalid,              // Shader is invalid, no source code associated with it.
        CompiledSuccessfully, // Shader has valid source code and is compiled.
        CompilationFailed,    // Shader has valid source code but compilation failed.
        PendingRemoval,       // This shader is to be removed during next update call.
        PendingCompilation    // This shader is to be compiled during the next update call.
    };

    // Compilation callback function, called by platform specific code to actually compile ths
    // shader on device.
    using CompileShader = function<bool(const MaterialShader& shader)>;

    // Destruction callback function, called by platform specific code to release compiled shader
    // binaries. By the time this is called the MaterialShader object is deleted so only index
    // within library is passed.
    using DestroyShader = function<void(int index)>;

    // Ctor takes the default set of entry points for the shaders in library.
    MaterialShaderLibrary(const vector<string>& defaultEntryPoints = {}) :
        _defaultEntryPoints(defaultEntryPoints)
    {
    }

    ~MaterialShaderLibrary();

    // Get IDs for all the active shaders in the library.
    vector<string> getActiveShaderIDs();

    // Get shared pointer to the shader with the given ID, or nullptr if none.
    MaterialShaderPtr get(const string& id);

    // Get shared pointer to the shader with the given libray index, or nullptr if none.
    MaterialShaderPtr get(int index);

    // Acquire a shader for the provided definition and set of entry points.
    // Will create a new one if none found with this definition's ID, otherwise will return a
    // previously created shader. If the entry point array is empty the default entry points for
    // this library are used.
    MaterialShaderPtr acquire(
        const MaterialShaderDefinition& def, const vector<string>& entryPoints = {});

    // Update the library and execute the provided compilation and destruction functions as
    // required, for shared pending recompilation or destruction. Will return false if no shaders
    // require compilation. Upon return from this function the lists of shaders pending compilation
    // or destruction
    bool update(CompileShader compileFunction, DestroyShader destroyFunction);

    // Force a rebuild of all the shaders in the library.
    void forceRebuildAll();

    // Returns true if there are shaders pending recompilation.
    bool rebuildRequired() { return !_shadersToCompile.empty(); }

protected:
    void destructionRequired(const MaterialShader& shader);
    void compilationRequired(const MaterialShader& shader);

    using WeakShaderPtr = weak_ptr<MaterialShader>;
    map<string, WeakShaderPtr> _shaders;
    vector<pair<WeakShaderPtr, CompileState>> _shaderState;
    set<int> _shadersToRemove;
    set<int> _shadersToCompile;

    vector<string> _defaultEntryPoints;

    list<int> _indexFreeList;
};

END_AURORA
