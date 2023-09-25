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

#include "MaterialBase.h"
#include "MaterialShader.h"
#include "Transpiler.h"

BEGIN_AURORA

// Alias for DirectX shader ID, used to access the shader functions in the library.
using DirectXShaderIdentifier = void*;

class MaterialBase;
struct MaterialDefaultValues;
struct CompileJob;

#define kDefaultShaderIndex 0

// The DX data for a compiled.
struct CompiledShader
{
    ComPtr<IDxcBlob> binary = nullptr;
    string id;
    string hlslFilename;
    string setupFunctionDeclaration;
    map<string, string> entryPoints;
    void destroyBinary() { binary = nullptr; }
    // Reset the struct (as indices are re-used.)
    void reset()
    {
        destroyBinary();
        entryPoints.clear();
        id.clear();
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

// Types of shader entry point used in the library.
struct EntryPointTypes
{
    static const string kInitializeMaterialExport;
};

/**
 * \desc Class representing a DXIL shader library, and its HLSL source code. The DXIL library and
 * its associated pipeline state must be rebuilt and linked as the source code for the shaders in
 * the library changes.  Uses the platform-indpendent class MaterialShaderLibrary to manage the
 * shaders themselves.
 */
class PTShaderLibrary
{
    // Shader library manages the material shader, and accesses private properties.
    friend class MaterialShader;

public:
    // Array of entry point names.
    static const vector<string> DefaultEntryPoints;

    /*** Lifetime Management ***/

    /**
     *  \param pDevice DirectX12 device used by the library.
     */
    PTShaderLibrary(ID3D12Device5Ptr device) :
        _pDXDevice(device), _shaderLibrary(DefaultEntryPoints)
    {
        initialize();
    }
    ~PTShaderLibrary() {}

    static const LPWSTR kInstanceHitGroupName;
    static const LPWSTR kInstanceClosestHitEntryPointName;
    static const LPWSTR kInstanceShadowAnyHitEntryPointName;
    static const LPWSTR kRayGenEntryPointName;
    static const LPWSTR kShadowMissEntryPointName;

    /// Get the named built-in material shader.  These are hand-coded and do not require runtime
    /// code generation, so this will never trigger a rebuild.
    MaterialShaderPtr getBuiltInShader(const string& name);

    /// Get the definition of the named built-in material shader.
    shared_ptr<MaterialDefinition> getBuiltInMaterialDefinition(const string& name);

    /**
     *  \desc Acquire a material shader for the provided definition.  This
     * will create new material shader (and trigger a rebuild) if a shader with the definition's ID
     * does not already exist.
     *
     *  \param def the definition of the material to acquire shader for.
     */
    MaterialShaderPtr acquireShader(const MaterialDefinition& def)
    {
        // Get the shader definition from the material defintion.
        // The shader definition is a subset of the material definition.
        MaterialShaderDefinition shaderDef;
        def.getShaderDefinition(shaderDef);

        // Acquire from the MaterialShaderLibrary object.
        return _shaderLibrary.acquire(shaderDef);
    }

    /// Get the DX pipeline state for this library. This will assert if the library requires a
    /// rebuild.
    ID3D12StateObjectPtr pipelineState()
    {
        AU_ASSERT(!rebuildRequired(),
            "Shader Library rebuild required, call rebuild() before accessing pipeline state.");
        return _pPipelineState;
    }

    /// Get the names of the built-in material types.
    const vector<string>& builtInMaterials() const { return _builtInMaterialNames; }

    /// Get the global root signature used by all shaders.
    ID3D12RootSignaturePtr globalRootSignature() const { return _pGlobalRootSignature; }

    /// Rebuild the shader library and its pipeline state. GPU must be idle before calling this.
    void rebuild(int globalTextureCount);

    // Get the DirectX shader reflection for library.
    ID3D12LibraryReflection* reflection() const { return _pShaderLibraryReflection; }

    // Set the named option to a given int value.
    // Will be added to shader library as a #define statement.
    bool setOption(const string& name, int value);

    // Get the DirectX shader ID for the provided shader.
    DirectXShaderIdentifier getShaderID(LPWSTR name);

    bool rebuildRequired() { return _shaderLibrary.rebuildRequired(); }

private:
    // Initialize the library.
    void initialize();

    void generateEvaluateMaterialFunction(CompileJob& jobOut);

    /*** Private Functions ***/

    void setupCompileJobForShader(const MaterialShader& shader, CompileJob& shadersOut);

    // Compile HLSL source code using DXCompiler.
    bool compileLibrary(const ComPtr<IDxcLibrary>& pDXCLibrary, const string source,
        const string& name, const string& target, const string& entryPoint,
        const vector<pair<wstring, string>>& defines, bool debug, ComPtr<IDxcBlob>& pOutput,
        string* pErrorMessage);

    bool linkLibrary(const vector<ComPtr<IDxcBlob>>& input, const vector<string>& inputNames,
        const string& target, const string& entryPoint, bool debug, ComPtr<IDxcBlob>& pOutput,
        string* pErrorMessage);

    void handleError(const string& errorMessage);

    void dumpShaderSource(const string& source, const string& name);

    // Create a DirectX root signature.
    ID3D12RootSignaturePtr createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);

    // Initialize the shared root signatures.
    void initRootSignatures(int globalTextureCount);

    // Remove the HLSL source for the associated index.  Called by friend class MaterialShader.
    void removeSource(int sourceIndex);

    CompiledShader& getDefaultShader() { return _compiledShaders[kDefaultShaderIndex]; }

    /*** Private Variables ***/

    vector<string> _builtInMaterialNames;

    ID3D12Device5Ptr _pDXDevice;

    ID3D12RootSignaturePtr _pGlobalRootSignature;
    ID3D12RootSignaturePtr _pRayGenRootSignature;
    ID3D12RootSignaturePtr _pInstanceHitRootSignature;

    ID3D12StateObjectPtr _pPipelineState;

    ID3D12LibraryReflection* _pShaderLibraryReflection;
    ComPtr<IDxcLibrary> _pDXCLibrary;
    ComPtr<IDxcCompiler> _pDXCompiler;
    ComPtr<IDxcLinker> _pDXLinker;

    MaterialShaderLibrary _shaderLibrary;
    map<string, shared_ptr<MaterialDefinition>> _builtInMaterialDefinitions;
    map<string, shared_ptr<MaterialShader>> _builtInShaders;

    vector<CompiledShader> _compiledShaders;
    string _optionsSource;
    PTShaderOptions _options;
    vector<shared_ptr<Transpiler>> _transpilerArray;

    Foundation::CPUTimer _timer;
    int _globalTextureCount = 0;
    ComPtr<IDxcBlob> _pDefaultShaderDXIL;
    string _defaultOptions;
};

END_AURORA
