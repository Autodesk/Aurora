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

#include "MaterialBase.h"

BEGIN_AURORA

class Transpiler;

// Alias for DirectX shader ID, used to access the shader functions in the library.
using DirectXShaderIdentifier = void*;

/**
 * \desc Class representing a material type, which maps directly to a DirectX hit group and a hit
 * shader in the HLSL shader library. Material types are managed by the shader library, and delete
 * via shared pointer when no longer referenced by any materials.
 */
class PTMaterialType
{
    // Shader library manages the material type, and accesses private properties.
    friend class PTShaderLibrary;

public:
    /**
     *  \param pShaderLibrary The shader library this type is part of.
     *  \param sourceIndex The index for this type's shader code within the library's HLSL source.
     *  \param hitEntryPoint The entry point for this type's hit shader.
     */
    PTMaterialType(PTShaderLibrary* pShaderLibrary, int sourceIndex, const string& typeName);
    ~PTMaterialType();

    // Entry point types.
    enum EntryPoint
    {
        kRadianceHit,
        kLayerMiss,
        kNumEntryPoints
    };

    // Array of entry point names.
    static constexpr string_view EntryPointNames[EntryPoint::kNumEntryPoints] = { "RADIANCE_HIT",
        "LAYER_MISS" };

    // Gets the DX shader identifier for this type's hit group.
    DirectXShaderIdentifier getShaderID();

    // Gets the DX shader identifier for this type's layer miss shader.
    DirectXShaderIdentifier getLayerShaderID();

    // Gets the unique material type name.
    const string& name() { return _name; }

    // Gets the export name for this type's hit group.
    const wstring& exportName() const { return _exportName; }

    // Gets the closest hit HLSL function entry point name for this type.
    const wstring& closestHitEntryPoint() const { return _closestHitEntryPoint; }

    // Gets the shadow any hit HLSL function entry point name for this type.
    const wstring& shadowAnyHitEntryPoint() const { return _shadowAnyHitEntryPoint; }

    // Gets the layer material miss shader HLSL function entry point name for this type.
    const wstring& materialLayerMissEntryPoint() const { return _materialLayerMissEntryPoint; }

    // Gets the index for this type's hit shader within the library's HLSL source.
    int sourceIndex() const { return _sourceIndex; }

    void incrementRefCount(EntryPoint entryPoint);

    void decrementRefCount(EntryPoint entryPoint);

    int refCount(EntryPoint entryPoint) const { return _entryPointRefCount[entryPoint]; }

    void getEntryPoints(map<string, bool>& entryPoints)
    {
        for (int i = 0; i < kNumEntryPoints; i++)
        {
            entryPoints[EntryPointNames[i].data()] = (refCount((EntryPoint)i) > 0);
        }
    }

protected:
    // Invalidate this type, by setting its shader library to null pointer.
    // Called by shader library in its destructor to avoid orphaned material types.
    void invalidate() { _pShaderLibrary = nullptr; }

    // Is this type still valid?
    bool isValid() { return _pShaderLibrary != nullptr; }

    array<int, EntryPoint::kNumEntryPoints> _entryPointRefCount;

    // The shader library this type is part of.
    PTShaderLibrary* _pShaderLibrary;

    // The index for this type's shader code within the library's HLSL source.
    int _sourceIndex;

    // Hit group export name.
    wstring _exportName;

    // The closest hit function entry point name for this type.
    wstring _closestHitEntryPoint;

    // The shadow any hit function entry point name for this type.
    wstring _shadowAnyHitEntryPoint;

    // The layer material miss shader entry for for this type.
    // TODO: Should only exist if used as layer material.
    wstring _materialLayerMissEntryPoint;

    // The unique material type name.
    string _name;
};

// Shared pointer type for material types.
using PTMaterialTypePtr = shared_ptr<PTMaterialType>;
struct CompiledMaterialType
{
    MaterialTypeSource source;
    ComPtr<IDxcBlob> binary = nullptr;
    void reset()
    {
        binary = nullptr;
        source.reset();
    }
};

// Shader options represented as set of HLSL #define statements.
class PTShaderOptions
{
public:
    // Remove named option.
    void remove(const string& name);

    // Set named option as boolean (set as 0 or 1 int).
    bool set(const string& name, bool val) { return set(name, val ? 1 : 0); }

    // Set named option as integer.
    bool set(const string& name, int val);

    // Clear all the options.
    void clear();

    // Get all the options as HLSL string.
    string toHLSL() const;

private:
    unordered_map<string, size_t> _lookup;
    vector<pair<string, int>> _data;
};

/**
 * \desc Class representing a DXIL shader library, and its HLSL source code.  Manages the material
 * types that implement Aurora materials using the compiled code in the library. The DXIL library
 * and its associated pipeline state must be rebuilt as the source code for the library changes.
 */
class PTShaderLibrary
{
    // Shader library manages the material type, and accesses private properties.
    friend class PTMaterialType;

public:
    /*** Lifetime Management ***/

    /**
     *  \param pDevice DirectX12 device used by the library.
     */
    PTShaderLibrary(ID3D12Device5Ptr device) : _pDXDevice(device) { initialize(); }
    ~PTShaderLibrary();

    /// Get the named built-in material type.  These are hand-coded and do not require runtime code
    /// generation, so this will never trigger a rebuild.
    PTMaterialTypePtr getBuiltInMaterialType(const string& name);

    void assembleShadersForMaterialType(const MaterialTypeSource& source,
        const map<string, bool>& entryPoints, vector<string>& hlslOut);

    /**
     *  \desc Acquire a material type for the provided source and type name.  This
     * will create new material type (and trigger a rebuild) if the named function does not already
     * exist..
     *
     *  \param entryPoint HLSL function name of closest hit shader for this material type, if type
     * with this name already exists will return that.
     *  \param hlslSource HLSL source code that implements this material type.  If an existing
     * material type for the entry point exists, the source code must match or this will trigger
     * assert.
     */
    PTMaterialTypePtr acquireMaterialType(
        const MaterialTypeSource& source, bool* pCreateNewType = nullptr);

    /// Get the DX pipeline state for this library. This will assert if the library requires a
    /// rebuild.
    ID3D12StateObjectPtr pipelineState()
    {
        AU_ASSERT(!_rebuildRequired,
            "Shader Library rebuild required, call rebuild() before accessing pipeline state.");
        return _pPipelineState;
    }

    /// Get the DX background miss shader identifier, that is shared by all material types.This will
    /// assert if the library requires a rebuild.
    DirectXShaderIdentifier getBackgroundMissShaderID();

    /// Get the DX radiance miss shader identifier, that is shared by all material types.This will
    /// assert if the library requires a rebuild.
    DirectXShaderIdentifier getRadianceMissShaderID();

    /// Get the DX shadow miss shader identifier, that is shared by all material types.This will
    /// assert if the library requires a rebuild.
    DirectXShaderIdentifier getShadowMissShaderID();

    /// Get the DX ray generation shader identifier, that is shared by all material types.This will
    /// assert if the library requires a rebuild.
    DirectXShaderIdentifier getRayGenShaderID();

    /// Get the names of the built-in material types.
    const vector<string>& builtInMaterials() const { return _builtInMaterialNames; }

    /// Get the global root signature used by all shaders.
    ID3D12RootSignaturePtr globalRootSignature() const { return _pGlobalRootSignature; }

    /// Is a rebuild of the shader library and its pipeline state required?
    bool rebuildRequired() { return _rebuildRequired; }

    /// Rebuild the shader library and its pipeline state. GPU must be idle before calling this.
    void rebuild();

    /// Set the MaterialX definition HLSL source. This will trigger rebuild if the new source is
    /// different to the current definitions source.
    bool setDefinitionsHLSL(const string& definitions);

    // Get the DirectX shader reflection for library.
    ID3D12LibraryReflection* reflection() const { return _pShaderLibraryReflection; }

    // Set the named option to a given int value.
    // Will be added to shader library as a #define statement.
    bool setOption(const string& name, int value);

    PTMaterialTypePtr getType(const string& name);

    vector<string> getActiveTypeNames();

    void triggerRebuild() { _rebuildRequired = true; }

private:
    /*** Private Functions ***/

    // Initialize the library.
    void initialize();

    // Compile HLSL source code using DXCompiler.
    bool compileLibrary(const ComPtr<IDxcLibrary>& pDXCLibrary, const string source,
        const string& name, const string& target, const string& entryPoint,
        const vector<pair<wstring, string>>& defines, bool debug, ComPtr<IDxcBlob>& pOutput,
        string* pErrorMessage);

    bool linkLibrary(const vector<ComPtr<IDxcBlob>>& input, const vector<string>& inputNames,
        const string& target, const string& entryPoint, bool debug, ComPtr<IDxcBlob>& pOutput,
        string* pErrorMessage);

    // Create a DirectX root signature.
    ID3D12RootSignaturePtr createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);

    // Get the shader identifier for an entry point.
    DirectXShaderIdentifier getShaderID(const wchar_t* entryPoint);

    // Initialize the shared root signatures.
    void initRootSignatures();

    // Remove the HLSL source for the associated index.  Called by friend class PTMaterialType.
    void removeSource(int sourceIndex);

    /*** Private Variables ***/
    vector<string> _builtInMaterialNames;

    ID3D12Device5Ptr _pDXDevice;

    ID3D12RootSignaturePtr _pGlobalRootSignature;
    ID3D12RootSignaturePtr _pRayGenRootSignature;
    ID3D12RootSignaturePtr _pRadianceHitRootSignature;
    ID3D12RootSignaturePtr _pLayerMissRootSignature;
    ;

    ID3D12StateObjectPtr _pPipelineState;

    ID3D12LibraryReflection* _pShaderLibraryReflection;
    ComPtr<IDxcLibrary> _pDXCLibrary;
    ComPtr<IDxcCompiler> _pDXCompiler;
    ComPtr<IDxcLinker> _pDXLinker;

    vector<int> _sourceToRemove;

    map<string, weak_ptr<PTMaterialType>> _materialTypes;
    map<string, PTMaterialTypePtr> _builtInMaterialTypes;

    bool _rebuildRequired = true;

    vector<CompiledMaterialType> _compiledMaterialTypes;
    string _materialXDefinitionsSource;
    string _optionsSource;
    PTShaderOptions _options;
    shared_ptr<Transpiler> _pTranspiler;

    Foundation::CPUTimer _timer;
};

END_AURORA
