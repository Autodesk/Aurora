//
// Copyright 2023 by Autodesk, Inc.  All rights reserved.
//
// This computer source code and related instructions and comments
// are the unpublished confidential and proprietary information of
// Autodesk, Inc. and are protected under applicable copyright and
// trade secret law.  They may not be disclosed to, copied or used
// by any third party without the prior written consent of Autodesk, Inc.
//
#include "pch.h"

#include "MaterialShader.h"

BEGIN_AURORA

MaterialShader::MaterialShader(MaterialShaderLibrary* pShaderLibrary, int libraryIndex,
    const MaterialShaderDefinition& def, const vector<string> entryPoints) :
    _pShaderLibrary(pShaderLibrary),
    _libraryIndex(libraryIndex),
    _entryPointsTypes(entryPoints),
    _def(def)
{

    // Initialize ref. counts to zero.
    for (int i = 0; i < entryPoints.size(); i++)
    {
        _entryPointNameLookup[entryPoints[i]] = i;
        _entryPoints.push_back({ entryPoints[i], 0 });
    }
}

MaterialShader::~MaterialShader()
{
    // If this material shader is valid, when its destroyed (which will happen when no material
    // holds a shared pointer to it) remove it source code from the library.
    if (isValid())
    {
        _pShaderLibrary->destructionRequired(*this);
    }
}

void MaterialShader::incrementRefCount(const string& entryPoint)
{
    EntryPoint* pEP = getEntryPoint(entryPoint);
    if (!pEP)
    {
        AU_ERROR("Unknown entry point %s", entryPoint.c_str());
        return;
    }
    pEP->refCount++;
    if (_pShaderLibrary && pEP->refCount == 1)
        _pShaderLibrary->compilationRequired(*this);
}

void MaterialShader::decrementRefCount(const string& entryPoint)
{
    EntryPoint* pEP = getEntryPoint(entryPoint);
    if (!pEP)
    {
        AU_ERROR("Unknown entry point %s", entryPoint.c_str());
        return;
    }
    AU_ASSERT(pEP->refCount > 0, "Invalid ref count");
    pEP->refCount--;
    if (_pShaderLibrary && pEP->refCount == 0)
        _pShaderLibrary->compilationRequired(*this);
}

int MaterialShader::refCount(const string& entryPoint) const
{
    const EntryPoint* pEP = getEntryPoint(entryPoint);
    if (!pEP)
    {
        AU_ERROR("Unknown entry point %s", entryPoint.c_str());
        return 0;
    }
    return pEP->refCount;
}

MaterialShaderLibrary::~MaterialShaderLibrary()
{
    // Invalidate any remaining valid material types, to avoid zombies.
    for (auto pWeakShaderPtr : _shaders)
    {
        MaterialShaderPtr pShader = pWeakShaderPtr.second.lock();
        if (pShader)
        {
            pShader->invalidate();
        }
    }
}

MaterialShaderPtr MaterialShaderLibrary::get(const string& id)
{
    return _shaders[id].lock();
}

MaterialShaderPtr MaterialShaderLibrary::get(int index)
{
    return _shaderState[index].first.lock();
}

vector<string> MaterialShaderLibrary::getActiveShaderIDs()
{
    vector<string> res;
    for (auto hgIter = _shaders.begin(); hgIter != _shaders.end(); hgIter++)
    {
        // Weak pointer is stored in shader library, ensure it has not been deleted.
        MaterialShaderPtr pShader = hgIter->second.lock();
        if (pShader)
        {
            res.push_back(pShader->id());
        }
    }
    return res;
}

void MaterialShaderLibrary::destructionRequired(const MaterialShader& shader)
{
    // Push index into pending removal list, the actual remove only happens when library rebuilt.
    int index = shader.libraryIndex();
    _shadersToRemove.insert(index);

    // Update state.
    _shaderState[index].second = CompileState::PendingRemoval;
}

void MaterialShaderLibrary::compilationRequired(const MaterialShader& shader)
{
    int index = shader.libraryIndex();
    // Push index in to vector, the actual remove only happens when library rebuilt.
    _shadersToCompile.insert(index);
    _shaderState[index].second = CompileState::PendingCompilation;
}

MaterialShaderPtr MaterialShaderLibrary::acquire(
    const MaterialShaderDefinition& def, const vector<string>& entryPoints)
{
    // The shared pointer to material shader.
    MaterialShaderPtr pShader;

    // Get the entry points (or the default entry points, if none provided)
    const vector<string>& ep = entryPoints.empty() ? _defaultEntryPoints : entryPoints;

    // Get the unique name from the source object.
    string id = def.source.uniqueId;

    // First see a material shader already exists.
    map<string, weak_ptr<MaterialShader>>::iterator hgIter = _shaders.find(id);
    if (hgIter != _shaders.end())
    {
        // Weak pointer is stored in shader library, ensure it has not been deleted.
        pShader = hgIter->second.lock();
        if (pShader)
        {
            // If the entry point exists, in debug mode do a string comparison to ensure the source
            // also matches.
            AU_ASSERT_DEBUG(def.compareSource(pShader->definition()),
                "Source mis-match for material shader %s.", pShader->id().c_str());

            // Ensure definitions match.
            AU_ASSERT(def.compare(pShader->definition()),
                "Definition mis-match for material shader %s.", pShader->id().c_str());

            // Ensure the entry points match.
            AU_ASSERT(pShader->entryPoints().size() == ep.size(), "Material entry points mismatch");

            // Return the existing material shader.
            return pShader;
        }
    }

    // Get a library index for new shader.
    int libIndex;
    if (!_indexFreeList.empty())
    {
        // Use the free list to reuse an index from a destroyed shader, if not empty.
        libIndex = _indexFreeList.back();
        _indexFreeList.pop_back();
    }
    else
    {
        // Add a new state entry if nothing in free list.
        libIndex = static_cast<int>(_shaderState.size());
        _shaderState.push_back({ weak_ptr<MaterialShader>(), CompileState::Invalid });
    }

    // Create new material shader.
    pShader = make_shared<MaterialShader>(this, libIndex, def, ep);

    // Fill in the shader state for the new shader.
    auto weakPtr                  = weak_ptr<MaterialShader>(pShader);
    _shaderState[libIndex].first  = weakPtr;
    _shaderState[libIndex].second = CompileState::Invalid;

    // Trigger compilation for the new shader.
    compilationRequired(*pShader.get());

    // Add weak reference to map.
    _shaders[def.source.uniqueId] = weakPtr;

    // Return the new material shader.
    return pShader;
}

bool MaterialShaderLibrary::update(CompileShader compileFunction, DestroyShader destroyFunction)
{
    // Destroy all shaders pending removal.
    for (auto iter = _shadersToRemove.begin(); iter != _shadersToRemove.end(); iter++)
    {
        // Run the provided callback function to destroy device-specific shader binary.
        int idx = *iter;
        destroyFunction(*iter);

        // Update the shader state, and add to free list.
        _shaderState[idx] = { weak_ptr<MaterialShader>(), CompileState::Invalid };
        _indexFreeList.push_back(idx);
    }

    // Clear pending removals.
    _shadersToRemove.clear();

    // Return value is true if there are shaders to compile.
    bool shadersCompiled = !_shadersToCompile.empty();

    // Compile all shaders pending compilation.
    for (auto iter = _shadersToCompile.begin(); iter != _shadersToCompile.end(); iter++)
    {
        // Get the shader, ensure there is still valid shader for this index.
        int idx      = *iter;
        auto pShader = _shaderState[idx].first.lock();

        if (pShader)
        {
            // Run the device-specific compilation callback function and update state based on
            // result.
            // TODO: Better handling of shader errors, currently will assert before reaching this
            // code on failure.
            bool res = compileFunction(*pShader.get());
            _shaderState[idx].second =
                res ? CompileState::CompiledSuccessfully : CompileState::CompilationFailed;
        }
    }

    // Clear pending compilations.
    _shadersToCompile.clear();

    // Return true if some shaders compiled.
    return shadersCompiled;
}

void MaterialShaderLibrary::forceRebuildAll()
{
    // Add all active shaders to compilation pending list.
    for (int i = 0; i < _shaderState.size(); i++)
    {
        MaterialShaderPtr pShader = _shaderState[i].first.lock();
        if (pShader)
            compilationRequired(*pShader.get());
    }
}

END_AURORA
