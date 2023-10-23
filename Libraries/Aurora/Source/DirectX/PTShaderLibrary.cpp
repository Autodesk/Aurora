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

#include "PTShaderLibrary.h"

#include "CompiledShaders/CommonShaders.h"
#include "CompiledShaders/MainEntryPoints.h"
#include "PTGeometry.h"
#include "PTImage.h"
#include "PTMaterial.h"
#include "PTRenderer.h"
#include "PTScene.h"
#include "PTShaderLibrary.h"
#include "PTTarget.h"
#include "Transpiler.h"

// Development flag to enable/disable multithreaded compilation.
#define AU_DEV_MULTITHREAD_COMPILATION 0

// Dump individual compilation times for each shader.
#define AU_DEV_DUMP_INDIVIDUAL_COMPILATION_TIME 0

#if AU_DEV_MULTITHREAD_COMPILATION
#include <execution>
#endif

BEGIN_AURORA
// Input to thread used to compile shaders.
struct CompileJob
{
    CompileJob(int ji) : jobIndex(ji) {}

    string code;
    string libName;
    map<string, string> includes;
    vector<pair<string, string>> entryPoints;
    int index;
    int jobIndex;
};

// Development flag to entire HLSL library to disk.
// NOTE: This should never be enabled in committed code; it is only for local development.
#define AU_DEV_DUMP_SHADER_CODE 0

// Development flag to transpiled HLSL library to disk.
// NOTE: This should never be enabled in committed code; it is only for local development.
#define AU_DEV_DUMP_TRANSPILED_CODE 0

// Pack four bytes into a single unsigned int.
// Based on version in DirectX toolkit.
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                             \
    (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) |                                            \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) |                                  \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) |                                 \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))

// Define the entry point type strings.
const string EntryPointTypes::kInitializeMaterialExport = "INITIALIZE_MATERIAL_EXPORT";

// Array of entry point names.
const vector<string> PTShaderLibrary::DefaultEntryPoints = {
    EntryPointTypes::kInitializeMaterialExport
};

// DXC include handler, used by PTShaderLibrary::compileLibrary
class IncludeHandler : public IDxcIncludeHandler
{
public:
    // Ctor takes the DXC library, and a map to lookup include files.
    IncludeHandler(ComPtr<IDxcLibrary> pDXCLibrary, const map<string, const string&>& includes) :
        _pDXCLibrary(pDXCLibrary), _includes(includes)
    {
    }

protected:
    // The LoadSource method is called by the DXC compiler to load an include file.
    HRESULT LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
    {
        // Convert filename to UTF8.
        string fname = Foundation::w2s(wstring(pFilename));

        // Strip the slashes from start of filename.
        int pathChars = 0;
        while (pathChars < fname.size() &&
            (fname[pathChars] == '/' || fname[pathChars] == '\\' || fname[pathChars] == '.'))
        {
            pathChars++;
        }
        if (pathChars)
            fname.erase(0, pathChars);

        // If our include map doesn't contain file, set output to nullptr and return.
        if (_includes.find(fname) == _includes.end())
        {
            *ppIncludeSource = nullptr;
            return S_OK;
        }

        // Convert the source to a DXC blob.
        const string& source = _includes.at(fname);
        ComPtr<IDxcBlobEncoding> pEncodingIncludeSource;
        _pDXCLibrary->CreateBlobWithEncodingFromPinned(
            source.c_str(), (UINT32)source.size(), CP_UTF8, &pEncodingIncludeSource);

        // Set the output to blob.
        *ppIncludeSource = pEncodingIncludeSource.Detach();

        // Return success code.
        return S_OK;
    }

    // Ref counting interface from IUnknown required, but just stubbed out.
    HRESULT QueryInterface(const IID&, void**) override { return S_OK; }
    ULONG AddRef(void) override { return 0; }
    ULONG Release(void) override { return 0; }

private:
    ComPtr<IDxcLibrary> _pDXCLibrary;
    const map<string, const string&>& _includes;
};

bool PTShaderOptions::set(const string& name, int val)
{
    // Find name in lookup map.
    size_t idx = static_cast<size_t>(-1);
    auto iter  = _lookup.find(name);

    // If the option doesn't exist, add it.
    // NOTE: A separate _lookup map and _data array are used to maintain the order of emitted
    // #define statements.
    if (iter == _lookup.end())
    {
        _lookup[name] = _data.size();
        _data.push_back({ name, val });
        return true;
    }
    idx = iter->second;

    // Otherwise set existing value.
    bool changed      = _data[idx].second != val;
    _data[idx].second = val;

    // Return true if value has changed.
    return changed;
}

void PTShaderOptions::remove(const string& name)
{
    // Remove from lookup map and clear entry in vector.
    size_t idx = _lookup[name];
    _lookup.erase(name);
    _data[idx] = { "", 0 };
}

void PTShaderOptions::clear()
{
    // Clear map and vector.
    _data.clear();
    _lookup.clear();
}

string PTShaderOptions::toHLSL() const
{
    // Build HLSL string.
    string hlslStr;

    // Add each option as #defines statement.
    for (size_t i = 0; i < _data.size(); i++)
    {
        hlslStr += "#define " + _data[i].first + " " + to_string(_data[i].second) + "\n";
    }

    // Return HLSL.
    return hlslStr;
}

// Define entry point name constants.
const LPWSTR PTShaderLibrary::kInstanceHitGroupName             = L"InstanceClosestHitShaderGroup";
const LPWSTR PTShaderLibrary::kInstanceClosestHitEntryPointName = L"InstanceClosestHitShader";
const LPWSTR PTShaderLibrary::kInstanceShadowAnyHitEntryPointName = L"InstanceShadowAnyHitShader";
const LPWSTR PTShaderLibrary::kRayGenEntryPointName               = L"RayGenShader";
const LPWSTR PTShaderLibrary::kShadowMissEntryPointName           = L"ShadowMissShader";

bool PTShaderLibrary::compileLibrary(const ComPtr<IDxcLibrary>& pDXCLibrary, const string source,
    const string& name, const string& target, const string& entryPoint,
    const vector<pair<wstring, string>>& defines, bool debug, ComPtr<IDxcBlob>& pOutput,
    string* pErrorMessage)
{
    // Pass the auto-generated map of minified HLSL code to an include handler.
    IncludeHandler includeHandler(pDXCLibrary, CommonShaders::g_sDirectory);

    // Create blob from HLSL source.
    ComPtr<IDxcBlobEncoding> pSource;
    if (pDXCLibrary->CreateBlobWithEncodingFromPinned(
            source.c_str(), (uint32)source.size(), CP_UTF8, pSource.GetAddressOf()) != S_OK)
    {
        AU_ERROR("Failed to create blob.");
        return false;
    }

    // Build vector of argument flags to pass to compiler.
    vector<const wchar_t*> args;
    args.push_back(L"-WX"); // Warning as error.
    args.push_back(L"-Zi"); // Debug info
    if (debug)
    {
        args.push_back(L"-Qembed_debug"); // Embed debug info into the shader
        args.push_back(L"-Od");           // Disable optimization
    }
    else
    {
        args.push_back(L"-O3"); // Optimization level 3
    }

    // Build DXC wide-string preprocessor defines from defines argument.
    vector<DxcDefine> dxcDefines(defines.size());
    vector<wstring> defValues(defines.size());
    for (size_t i = 0; i < defines.size(); ++i)
    {
        // Store the wide-string value (DxcDefine points to this).
        defValues[i] = Foundation::s2w(defines[i].second.c_str());
        DxcDefine& m = dxcDefines[i];
        m.Name       = defines[i].first.c_str();
        m.Value      = defValues[i].c_str();
    }

    // Convert string arguments to wide-string.
    wstring nameWide       = Foundation::s2w(name);
    wstring entryPointWide = Foundation::s2w(entryPoint);
    wstring targetWide     = Foundation::s2w(target);

    // Run the compiler and get the result.
    ComPtr<IDxcOperationResult> pCompileResult;
    if (_pDXCompiler->Compile(pSource.Get(), nameWide.c_str(), entryPointWide.c_str(),
            targetWide.c_str(), (LPCWSTR*)&args[0], (unsigned int)args.size(), dxcDefines.data(),
            (uint32)dxcDefines.size(), &includeHandler, pCompileResult.GetAddressOf()) != S_OK)
    {
        // Print error and return false if DXC fails.
        AU_ERROR("DXCompiler compile failed");
        return false;
    }

    // Get the status of the compilation.
    HRESULT status;
    if (pCompileResult->GetStatus(&status) != S_OK)
    {
        AU_ERROR("Failed to get result.");
        return false;
    }

    // Get error buffer if status shows a failure.
    if (status < 0)
    {
        // Get error buffer to provided pErrorMessage string.
        ComPtr<IDxcBlobEncoding> pPrintBlob;
        if (pCompileResult->GetErrorBuffer(pPrintBlob.GetAddressOf()) != S_OK)
        {
            AU_ERROR("Failed to get error buffer.");
            return false;
        }
        *pErrorMessage = string((char*)pPrintBlob->GetBufferPointer(), pPrintBlob->GetBufferSize());
        return false;
    }

    // Get compiled shader as blob.
    IDxcBlob** pBlob = reinterpret_cast<IDxcBlob**>(pOutput.GetAddressOf());
    pCompileResult->GetResult(pBlob);
    return true;
}

bool PTShaderLibrary::linkLibrary(const vector<ComPtr<IDxcBlob>>& input,
    const vector<string>& inputNames, const string& target, const string& entryPoint, bool debug,
    ComPtr<IDxcBlob>& pOutput, string* pErrorMessage)
{

    // Build vector of argument flags to pass to compiler.
    vector<const wchar_t*> args;
    args.push_back(L"-WX"); // Warning as error.
    args.push_back(L"-Zi"); // Debug info
    if (debug)
    {
        args.push_back(L"-Qembed_debug"); // Embed debug info into the shader
        args.push_back(L"-Od");           // Disable optimization
    }
    else
    {
        args.push_back(L"-O3"); // Optimization level 3
    }

    // Convert string arguments to wide-string.
    wstring entryPointWide = Foundation::s2w(entryPoint);
    wstring targetWide     = Foundation::s2w(target);

    // Build vector of input names converted to wide string.
    vector<wstring> inputNamesStr;
    for (int i = 0; i < input.size(); i++)
    {
        wstring libName = Foundation::s2w(inputNames[i]);
        inputNamesStr.push_back(libName);
    }

    // Register each of the named inputs with the associated binary blob.
    vector<LPWSTR> inputNamesStrPtr;
    for (int i = 0; i < input.size(); i++)
    {
        // Build vector of string points to input names.
        LPWSTR strPtr = (LPWSTR)inputNamesStr[i].c_str();
        inputNamesStrPtr.push_back(strPtr);

        // Register the input binary with the given input name.
        auto pBlob = input[i].Get();
        if (_pDXLinker->RegisterLibrary(strPtr, pBlob) != S_OK)
        {
            // Print error and return false if DXC fails.
            AU_ERROR("Link RegisterLibrary failed");
            return false;
        }
    }

    // Run the linker and get the result.
    ComPtr<IDxcOperationResult> pLinkResult;
    if (_pDXLinker->Link(entryPointWide.c_str(), targetWide.c_str(), inputNamesStrPtr.data(),
            (unsigned int)inputNamesStrPtr.size(), (LPCWSTR*)&args[0], (unsigned int)args.size(),
            pLinkResult.GetAddressOf()) != S_OK)
    {
        // Print error and return false if DXC fails.
        AU_ERROR("DXCompiler link failed");
        return false;
    }

    // Get the status of the compilation.
    HRESULT status;
    if (pLinkResult->GetStatus(&status) != S_OK)
    {
        AU_ERROR("Failed to get result.");
        return false;
    }

    // Get error buffer if status shows a failure.
    if (status < 0)
    {
        // Get error buffer to provided pErrorMessage string.
        ComPtr<IDxcBlobEncoding> pPrintBlob;
        if (pLinkResult->GetErrorBuffer(pPrintBlob.GetAddressOf()) != S_OK)
        {
            AU_ERROR("Failed to get error buffer.");
            return false;
        }
        *pErrorMessage = string((char*)pPrintBlob->GetBufferPointer(), pPrintBlob->GetBufferSize());
        return false;
    }

    // Get linked shader as blob.
    IDxcBlob** pBlob = reinterpret_cast<IDxcBlob**>(pOutput.GetAddressOf());
    pLinkResult->GetResult(pBlob);
    return true;
}

ID3D12RootSignaturePtr PTShaderLibrary::createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    // Create a root signature from the specified description.
    ID3D12RootSignaturePtr pSignature;
    ID3DBlobPtr pSignatureBlob;
    ID3DBlobPtr pErrorBlob;
    checkHR(::D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob));
    checkHR(_pDXDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(),
        pSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pSignature)));

    return pSignature;
}

void PTShaderLibrary::initRootSignatures(int globalTextureCount)
{
    // Specify the global root signature for all shaders. This includes a global static sampler
    // which shaders can use by default, as well as dynamic samplers per-material.
    CD3DX12_DESCRIPTOR_RANGE texRange;

    // Create the global root signature.
    // Must match the root signature data setup in PTRenderer::submitRayDispatch and the GPU
    // version in GlobalRootSignature.slang.
    array<CD3DX12_ROOT_PARAMETER, 11> globalRootParameters = {}; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[0].InitAsShaderResourceView(0);         // gScene: acceleration structure
    globalRootParameters[1].InitAsConstants(2, 0);               // sampleIndex + seedOffset
    globalRootParameters[2].InitAsConstantBufferView(1); // gFrameData: per-frame constant buffer
    globalRootParameters[3].InitAsConstantBufferView(2); // gEnvironmentConstants
    globalRootParameters[4].InitAsShaderResourceView(1); // gEnvironmentAliasMap
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2,
        2); // gEnvironmentLight/BackgroundTexture (starting at t2)
    CD3DX12_DESCRIPTOR_RANGE globalRootRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[5].InitAsDescriptorTable(_countof(globalRootRanges), globalRootRanges);
    globalRootParameters[6].InitAsConstantBufferView(3); // gGroundPlane
    globalRootParameters[7].InitAsShaderResourceView(4); // gNullScene: null acceleration structure
    globalRootParameters[8].InitAsShaderResourceView(
        5); // gGlobalMaterialConstants: global material constants.
    globalRootParameters[9].InitAsShaderResourceView(
        6); // gGlobalInstanceConstants: global instance constants.

    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, globalTextureCount,
        7); // gGlobalMaterialTextures: Global scene textures.
    CD3DX12_DESCRIPTOR_RANGE sceneTextureRanges[] = {
        texRange
    }; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[10].InitAsDescriptorTable(
        _countof(sceneTextureRanges), sceneTextureRanges);

    // gDefault sampler.
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

    // Create the global root signature object.
    CD3DX12_ROOT_SIGNATURE_DESC globalDesc(static_cast<UINT>(globalRootParameters.size()),
        globalRootParameters.data(), 1, &samplerDesc);
    _pGlobalRootSignature = createRootSignature(globalDesc);
    _pGlobalRootSignature->SetName(L"Global Root Signature");

    // Specify a local root signature for the ray gen shader.
    // The AOV descriptor count defined here must match the AOV RWTexture2Ds defined in
    // MainEntryPoints.slang. and the descriptors uploaded to the heap in
    // PTRenderer::updateOutputResources and PTRenderer::updateDenoisingResources.
    // NOTE: The output UAVs are typed, and therefore can't be used with a root descriptor; they
    // must come from a descriptor heap.
    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 7, 0);   // Output images (for AOV data)
    CD3DX12_DESCRIPTOR_RANGE rayGenRanges[] = { uavRange }; // NOLINT(modernize-avoid-c-arrays)
    array<CD3DX12_ROOT_PARAMETER, 1> rayGenRootParameters = {};
    rayGenRootParameters[0].InitAsDescriptorTable(_countof(rayGenRanges), rayGenRanges);
    CD3DX12_ROOT_SIGNATURE_DESC rayGenDesc(
        static_cast<UINT>(rayGenRootParameters.size()), rayGenRootParameters.data());
    rayGenDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pRayGenRootSignature = createRootSignature(rayGenDesc);
    _pRayGenRootSignature->SetName(L"Ray Gen Local Root Signature");

    // Start a local root signature for the instance hit group (that is shared by all instances.)
    // Must match the GPU layout defined in InstancePipelineState.slang and the HitGroupShaderRecord
    // structure.
    array<CD3DX12_ROOT_PARAMETER, 7> instanceHitParameters = {};

    // Geometry buffers: indices, positions, normals, tangents, and texture coordinates.
    instanceHitParameters[0].InitAsShaderResourceView(0, 1); // gIndices
    instanceHitParameters[1].InitAsShaderResourceView(1, 1); // gPositions
    instanceHitParameters[2].InitAsShaderResourceView(2, 1); // gNormals
    instanceHitParameters[3].InitAsShaderResourceView(3, 1); // gTangents
    instanceHitParameters[4].InitAsShaderResourceView(4, 1); // gTexCoords

    // Constants from HitGroupShaderRecord: gHasNormals, gHasTangents, gHasTexCoords,
    // gIsOpaque, and gInstanceBufferOffset.
    instanceHitParameters[5].InitAsConstants(5, 0, 1);

    // gMaterialLayerIDs: indices for layer material shaders.
    instanceHitParameters[6].InitAsConstantBufferView(1, 1);

    // Create the instance root signature.
    CD3DX12_ROOT_SIGNATURE_DESC instanceHitDesc(
        static_cast<UINT>(instanceHitParameters.size()), instanceHitParameters.data());
    instanceHitDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pInstanceHitRootSignature = createRootSignature(instanceHitDesc);
    _pInstanceHitRootSignature->SetName(L"Instance Hit Group Local Root Signature");
}

DirectXShaderIdentifier PTShaderLibrary::getShaderID(LPWSTR name)
{
    // Assert if a rebuild is required, as the pipeline state will be invalid.
    AU_ASSERT(!rebuildRequired(),
        "Shader Library rebuild required, call rebuild() before accessing shaders");

    // Get the shader ID from the pipeline state.
    ID3D12StateObjectPropertiesPtr stateObjectProps;
    _pPipelineState.As(&stateObjectProps);
    return stateObjectProps->GetShaderIdentifier(name);
}

// IDxcBlock interface to static pointer.
class StaticBlob : public IDxcBlob
{
public:
    StaticBlob(const void* data, size_t size) : _data(data), _size(size) {}
    virtual void* GetBufferPointer(void) override { return const_cast<void*>(_data); }
    virtual size_t GetBufferSize(void) override { return _size; }

    // Ref counting interface not needed for static buffer.
    virtual HRESULT QueryInterface(const IID&, void**) override { return S_OK; }
    virtual ULONG AddRef(void) override { return 0; }
    virtual ULONG Release(void) override { return 0; }

private:
    const void* _data;
    size_t _size;
};

void PTShaderLibrary::initialize()
{

    // Create a static block from the precomplied main entry point DXIL.
    _pDefaultShaderDXIL.Attach(new StaticBlob(g_sMainEntryPointsDXIL.data(),
        g_sMainEntryPointsDXIL.size() * sizeof(g_sMainEntryPointsDXIL[0])));

    // Create a string for the default shader options (remove the first line which is added by
    // minify script)
    _defaultOptions = CommonShaders::g_sOptions;
    _defaultOptions.erase(0, _defaultOptions.find("\n") + 1);

    // Always use runtime compiled evaluateMaterialForShader.
    // TODO: Use pre-compiled default version when only default shader is used in scene.
    _options.set("RUNTIME_COMPILE_EVALUATE_MATERIAL_FUNCTION", 1);

    _optionsSource = _options.toHLSL();

    // Create an emptry array of Slang transpilers.
    _transpilerArray = {};

    // Clear the source and built ins vector. Not strictly needed, but this function could be called
    // repeatedly in the future.
    _compiledShaders.clear();
    _builtInMaterialNames = {};

    // Create source code for the default shader, containg the main entry points used for all
    // shaders..
    MaterialShaderSource defaultMaterialSource("Default", CommonShaders::g_sMainEntryPoints);

    // Add the shared entry points to the default shader's definitions source.

    // Create the material definition for default shader.
    _builtInMaterialDefinitions[defaultMaterialSource.uniqueId] =
        make_shared<MaterialDefinition>(defaultMaterialSource,
            MaterialBase::StandardSurfaceDefaults, MaterialBase::updateBuiltInMaterial, false);

    // Create shader from the definition.
    MaterialShaderDefinition shaderDef;
    _builtInMaterialDefinitions[defaultMaterialSource.uniqueId]->getShaderDefinition(shaderDef);
    MaterialShaderPtr pDefaultShader = _shaderLibrary.acquire(shaderDef);

    // Ensure the instance hit entry point is compiled for default shader.
    pDefaultShader->incrementRefCount(EntryPointTypes::kInitializeMaterialExport);

    // Add default shader to the built-in array.
    _builtInMaterialNames.push_back(defaultMaterialSource.uniqueId);
    _builtInShaders[defaultMaterialSource.uniqueId] =
        pDefaultShader; // Stores strong reference to the built-in's shader.
}

bool PTShaderLibrary::setOption(const string& name, int value)
{
    // Set the option and see if actually changed.
    bool changed = _options.set(name, value);

    // If the option changed set the HLSL and trigger rebuild.
    if (changed)
    {
        _optionsSource = _options.toHLSL();
        _shaderLibrary.forceRebuildAll();
    }

    return changed;
}

MaterialShaderPtr PTShaderLibrary::getBuiltInShader(const string& name)
{
    return _builtInShaders[name];
}

shared_ptr<MaterialDefinition> PTShaderLibrary::getBuiltInMaterialDefinition(const string& name)
{
    return _builtInMaterialDefinitions[name];
}

void PTShaderLibrary::setupCompileJobForShader(const MaterialShader& shader, CompileJob& jobOut)
{
    // Add shared common code.
    auto& source = shader.definition().source;

    jobOut.code += "#include \"Options.slang\"\n";

    jobOut.code += "#include \"Definitions.slang\"\n\n";

    jobOut.code += source.setup;

    // Get the compiled shader for this material shader.
    auto& compiledShader = _compiledShaders[shader.libraryIndex()];

    // Set the library name to the shader name.
    jobOut.libName = compiledShader.hlslFilename;

    // Set the includes from the shader's source code and the options source.
    jobOut.includes = {
        { "Options.slang", _optionsSource },
        { "Definitions.slang", source.definitions },
    };

    // Set the entry points to be post-processed (Slang will remove the [shader] tags).
    jobOut.entryPoints = {};

    // Set the index to map back to the compiled shader array.
    jobOut.index = shader.libraryIndex();
}

void PTShaderLibrary::generateEvaluateMaterialFunction(CompileJob& job)
{
    // Add includes to source code.
    job.code = "#include \"Material.slang\"\n";
    job.code += "#include \"Geometry.slang\"\n";

    // Add declartion for default evaluate material function.
    job.code +=
        "Material evaluateDefaultMaterial(ShadingData shading, int offset, out float3 "
        "materialNormal, out bool isGeneratedNormal);\n";

    // Add declaration for each of the runtime compiled evaluate material functions (these will be
    // compiled seperately and linked in.)
    for (int i = 1; i < _compiledShaders.size(); i++)
    {
        auto& compiledShader = _compiledShaders[i];
        job.code += compiledShader.setupFunctionDeclaration + ";\n";
    }

    // Add evaluate function definition.
    job.code += R""""(
    export Material evaluateMaterialForShader(int shaderIndex, ShadingData shading, int offset, out float3 materialNormal,
        out bool isGeneratedNormal) {
)"""";

    // If there are multiple shaders to choose from, add a switch statment.
    if (_compiledShaders.size() > 1)
    {
        job.code += R""""(
            switch(shaderIndex) {

)"""";

        // Add condition to switch statement for each shader, that calls its evaluateMaterial
        // function.
        for (int i = 1; i < _compiledShaders.size(); i++)
        {
            auto& compiledShader = _compiledShaders[i];
            job.code += "\t\tcase " + to_string(i) + ":\n";
            job.code += "\t\t\treturn  evaluateMaterial_" + compiledShader.id +
                "(shading, offset, "
                "materialNormal, "
                "isGeneratedNormal);\n";
        }

        // Complete switch statement and call default evaluate material function in default case.
        job.code += R""""(
                default:
            }
)"""";
    }

    // Return default shader.
    job.code += R""""(
            return  evaluateDefaultMaterial(shading, offset, materialNormal,
                isGeneratedNormal);
    }
)"""";

    // Set the library name to the shader name.
    job.libName = "EvaluateMaterialFunction";

    // Set the options include.
    job.includes = {
        { "Options.slang", _optionsSource },
    };

    // No entry points to pre-process
    job.entryPoints = {};

    // Job index is -1 as this does not correspond to a materialShader in the shadersToLink array.
    job.index = -1;
}

void PTShaderLibrary::rebuild(int globalTextureCount)
{

    // Start timer.
    _timer.reset();

    // Initialize root signatures (these are shared by all shaders, and don't change.)
    initRootSignatures(globalTextureCount);

    // Should only be called if required (rebuilding requires stalling the GPU pipeline.)
    AU_ASSERT(rebuildRequired(), "Rebuild not needed");

    // Build vector of compile jobs to execute in parallel.
    vector<CompileJob> compileJobs;

    // Compile function is executed by MaterialShaderLibrary::update for any shaders that need
    // recompiling.
    auto setupShaderFunction = [this, &compileJobs](const MaterialShader& shader) {
        // If there is no compiled shader object in the array for this shader, then create one.
        if (_compiledShaders.size() <= shader.libraryIndex())
        {
            // Resize array of compiled shaders.
            _compiledShaders.resize(shader.libraryIndex() + 1);
        }

        // Get the compiled shader object for this shader, and destroy any existing compiled shader
        // binary.
        auto& compiledShader = _compiledShaders[shader.libraryIndex()];

        // If the compiled shader object is empty, fill in the entry points, etc.
        if (compiledShader.id.empty())
        {

            // Set the ID from the shader ID.
            _compiledShaders[shader.libraryIndex()].id           = shader.id();
            _compiledShaders[shader.libraryIndex()].hlslFilename = shader.id() + ".hlsl";
            _compiledShaders[shader.libraryIndex()].setupFunctionDeclaration =
                shader.definition().source.setupFunctionDeclaration;

            Foundation::sanitizeFileName(_compiledShaders[shader.libraryIndex()].hlslFilename);
        }

        // Destroy any existing compiled binary.
        compiledShader.destroyBinary();

        // Ensure ID matches shader.
        AU_ASSERT(compiledShader.id.compare(shader.id()) == 0, "Compiled shader mismatch");

        // Setup the compile job for this shader.
        compileJobs.push_back(CompileJob(int(compileJobs.size())));
        setupCompileJobForShader(shader, compileJobs.back());

        // If this is the default shader (which always library index 0), add the shared entry points
        // (used by all shaders)
        if (shader.libraryIndex() == kDefaultShaderIndex)
        {
            compileJobs.back().entryPoints.push_back(
                { "miss", Foundation::w2s(kShadowMissEntryPointName) });
            compileJobs.back().entryPoints.push_back(
                { "raygeneration", Foundation::w2s(kRayGenEntryPointName) });
            compileJobs.back().entryPoints.push_back(
                { "closesthit", Foundation::w2s(kInstanceClosestHitEntryPointName) });
            compileJobs.back().entryPoints.push_back(
                { "anyhit", Foundation::w2s(kInstanceShadowAnyHitEntryPointName) });
        }

        return true;
    };

    // Destroy function is executed by MaterialShaderLibrary::update for any shaders that need
    // releasing.
    auto destroyShaderFunction = [this](int index) {
        if (_compiledShaders.size() <= index)
            return;

        _compiledShaders[index].reset();
    };

    // Run the MaterialShaderLibrary update function and return if no shaders were compiled.
    if (!_shaderLibrary.update(setupShaderFunction, destroyShaderFunction))
        return;

    // Generate the evaluateMaterialForShader function, and add to compile jobs.
    compileJobs.push_back(CompileJob(int(compileJobs.size())));
    generateEvaluateMaterialFunction(compileJobs.back());

    // Binary for the compiled evaluateMaterial function.
    ComPtr<IDxcBlob> evaluateMaterialBinary;

    // If shaders were rebuild we must completely rebuild the pipeline state with the following
    // subjects:
    // - Pipeline configuration: the max trace recursion depth.
    // - DXIL library: the compiled shaders in a bundle.
    // - Shader configurations: the ray payload and intersection attributes sizes.
    // - Associations between shader configurations and specific shaders (NOT DONE HERE).
    // - The global root signature: inputs for all shaders.
    // - Any local root signatures: inputs for specific shaders.
    // - Associations between local root signatures and specific shaders.
    // - Hit groups: combinations of closest hit / any hit / intersection shaders

    // Create the DXC library, linker, and compiler.
    // NOTE: DXCompiler.dll is set DELAYLOAD in the linker settings, so this will abort if DLL
    // not available.
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(_pDXCLibrary.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(_pDXCompiler.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(_pDXLinker.GetAddressOf()));

    // As the transpiler is not thread safe we must create one for each thread.
    // TODO: These are around 30-40Mb each so we could look at cleaning them up once a certain
    // number are allocated.
    float scStart          = _timer.elapsed();
    size_t transpilerCount = AU_DEV_MULTITHREAD_COMPILATION ? compileJobs.size() : 1;
    for (size_t i = _transpilerArray.size(); i < transpilerCount; i++)
    {
        _transpilerArray.push_back(make_shared<Transpiler>(CommonShaders::g_sDirectory));
    }
    float scEnd = _timer.elapsed();

    // Transpilation and DXC Compile function is called from parallel threads.
    auto compileFunc = [this, &evaluateMaterialBinary](CompileJob& job) {
        // If this is the default shader and no shader options have been changed, use the
        // precompiled version.
        if (job.index == kDefaultShaderIndex && _defaultOptions.compare(_optionsSource) == 0)
        {
            // Set the compiled shader's binary to the pre-compiled DXIL blob.
            _compiledShaders[job.index].binary = _pDefaultShaderDXIL;

            // Return without running the compiler.
            return;
        }

#if AU_DEV_DUMP_INDIVIDUAL_COMPILATION_TIME
        float jobStart = _timer.elapsed();
#endif

        // Get the transpiler for this thread (in non-multithread case, there is only one.)
        int transpilerIndex = std::min(job.jobIndex, int(_transpilerArray.size()) - 1);
        auto pTranspiler    = _transpilerArray[transpilerIndex];

        // If development flag set dump HLSL library to a file.
        if (AU_DEV_DUMP_SHADER_CODE)
        {
            if (Foundation::writeStringToFile(job.code, job.libName))
                AU_INFO("Dumping shader code to:%s", job.libName.c_str());
            else
                AU_WARN("Failed to write shader code to:%s", job.libName.c_str());
        }

        // Set the shader source as source code available as via #include in the compiler.
        for (auto iter = job.includes.begin(); iter != job.includes.end(); iter++)
        {
            pTranspiler->setSource(iter->first, iter->second);
        }

        // Run the transpiler
        string transpilerErrors;
        string transpiledHLSL;
        if (!pTranspiler->transpileCode(
                job.code, transpiledHLSL, transpilerErrors, Transpiler::Language::HLSL))
        {
            AU_ERROR("Slang transpiling error log:\n%s", transpilerErrors.c_str());
            AU_DEBUG_BREAK();
            AU_FAIL("Slang transpiling failed, see log in console for details.");
        }

        // Fix up the entry points (Slang will remove all the [shader] tags except one. So we need
        // to re-add them.
        for (int i = 0; i < job.entryPoints.size(); i++)
        {
            // Build the code to search for and version with [shader] prefixed.
            string entryPointCode = "void " + job.entryPoints[i].second;
            string entryPointCodeWithTag =
                "[shader(\"" + job.entryPoints[i].first + "\")] " + entryPointCode;

            // Run the regex to replace add the tag to the transpiled HLSL source.
            transpiledHLSL = regex_replace(
                transpiledHLSL, regex("\n" + entryPointCode), "\n" + entryPointCodeWithTag);
        }

        // If development flag set dump transpiled library to a file.
        if (AU_DEV_DUMP_TRANSPILED_CODE)
        {
            if (Foundation::writeStringToFile(transpiledHLSL, job.libName))
                AU_INFO("Dumping transpiled code to:%s", job.libName.c_str());
            else
                AU_WARN("Failed to write transpiled code to:%s", job.libName.c_str());
        }

        // Compile the HLSL source for this shader.
        ComPtr<IDxcBlob> compiledShader;
        vector<pair<wstring, string>> defines = { { L"DIRECTX", "1" } };
        string errorMessage;
        if (!compileLibrary(_pDXCLibrary,
                transpiledHLSL.c_str(), // Source code string.
                job.libName,            // Arbitrary shader name
                "lib_6_3",              // Used DXIL 6.3 shader target
                "",                     // DXIL has empty entry point.
                defines,                // Defines (currently empty vector).
                false,                  // Don't use debug mode.
                compiledShader,         // Result as blob.
                &errorMessage           // Error message (if compilation fails.)
                ))
        {
            // Report a compile error and then break the debugger if attached. This gives the
            // developer a chance to handle HLSL programming errors as early as possible.
            AU_ERROR("HLSL compilation error log:\n%s", errorMessage.c_str());
            AU_DEBUG_BREAK();
            AU_FAIL("HLSL compilation failed for %s, see log in console for details.",
                job.libName.c_str());
        }

        // Set the compiled binary in the compiled shader obect for this shader.
        if (job.index < 0)
            evaluateMaterialBinary = compiledShader;
        else
            _compiledShaders[job.index].binary = compiledShader;

#if AU_DEV_DUMP_INDIVIDUAL_COMPILATION_TIME
        float jobEnd = _timer.elapsed();
        AU_INFO("Compiled shader %s in  %d ms", job.libName.c_str(),
            static_cast<int>(jobEnd - jobStart));
#endif
    };

    // Compile all the shaders in parallel (if AU_DEV_MULTITHREAD_COMPILATION is set.)
    float compStart = _timer.elapsed();
#if AU_DEV_MULTITHREAD_COMPILATION
    for_each(execution::par, compileJobs.begin(), compileJobs.end(),
        [compileFunc](CompileJob& job) { compileFunc(job); });
#else
    // Otherwise run in single thread.
    for (auto& j : compileJobs)
    {
        compileFunc(j);
    }
#endif
    float compEnd = _timer.elapsed();

    // Build array of all the shader binaries (not just the ones compiled this frame) and names for
    // linking.
    vector<ComPtr<IDxcBlob>> shadersToLink;
    vector<string> shaderNamesToLink;

    // Add the evaluate material shader blob and name to shader binaries to link.
    shadersToLink.push_back(evaluateMaterialBinary);
    shaderNamesToLink.push_back("EvaluateMaterialFunction");

    // Add the binaries for all the compiled material shaders.
    for (int i = 0; i < _compiledShaders.size(); i++)
    {
        auto& compiledShader = _compiledShaders[i];
        if (compiledShader.binary)
        {
            shadersToLink.push_back(compiledShader.binary);
            string libName = compiledShader.id + ".hlsl";
            Foundation::sanitizeFileName(libName);
            shaderNamesToLink.push_back(libName);
        }
    }

    // Link the compiled shaders into a single library blob.
    float linkStart = _timer.elapsed();
    ComPtr<IDxcBlob> linkedShader;
    string linkErrorMessage;
    if (!linkLibrary(shadersToLink, shaderNamesToLink,
            "lib_6_3",        // Used DXIL 6.3 shader target
            "",               // DXIL has empty entry point.
            false,            // Don't use debug mode.
            linkedShader,     // Result as blob.
            &linkErrorMessage // Error message (if compilation fails.)
            ))
    {
        // Report a compile error and then break the debugger if attached. This gives the
        // developer a chance to handle HLSL programming errors as early as possible.
        AU_ERROR("HLSL linker error log:\n%s", linkErrorMessage.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("HLSL compilation failed, see log in console for details.");
    }
    float linkEnd = _timer.elapsed();

    size_t libraryHash = Foundation::hashInts((uint32_t*)linkedShader->GetBufferPointer(),
        linkedShader->GetBufferSize() / sizeof(uint32_t));

    // Create bytecode from compiled blob.
    D3D12_SHADER_BYTECODE shaderByteCode =
        CD3DX12_SHADER_BYTECODE(linkedShader->GetBufferPointer(), linkedShader->GetBufferSize());

    // Prepare an empty pipeline state object description.
    CD3DX12_STATE_OBJECT_DESC pipelineStateDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Create a pipeline configuration subobject, which simply specifies the recursion depth.
    // NOTE: The recursion depth is set to 1 as we are using a non-recusive model with entire path
    // traced in the ray generation shader without recursion.
    auto* pPipelineConfigSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pPipelineConfigSubobject->Config(1);

    // Create a DXIL library subobject.
    // NOTE: Shaders are not subobjects, but the library containing them is a subobject. All of the
    // shader entry points are exported by default; if only specific entry points should be
    // exported, then DefineExport() on the subobject should be used.
    auto* pLibrarySubObject = pipelineStateDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

    // Set DXIL library object bytecode.
    pLibrarySubObject->SetDXILLibrary(&shaderByteCode);

    // Check shader material compatibility structure using reflection.
    // Get the DXC blob compiled in compileShader.
    IDxcBlob* pBlob = linkedShader.Get();

    // Create a container reflection object from the blob.
    ComPtr<IDxcContainerReflection> pContainerReflection;
    DxcCreateInstance(
        CLSID_DxcContainerReflection, IID_PPV_ARGS(pContainerReflection.GetAddressOf()));
    pContainerReflection->Load(pBlob);

    // Find the DXIL reflection object within the container.
    UINT32 shaderIdx;
    pContainerReflection->FindFirstPartKind(MAKEFOURCC('D', 'X', 'I', 'L'), &shaderIdx);
    pContainerReflection->GetPartReflection(
        shaderIdx, __uuidof(ID3D12LibraryReflection), (void**)&_pShaderLibraryReflection);

    // Create a shader configuration subobject, which indicates the maximum sizes of the ray payload
    // (as defined in the RayTrace.slang; in the ray payload structs "InstanceRayPayload" and
    // "ShadowRayPayload") and intersection attributes (UV barycentric coordinates).
    // If the structures used in the shader exceed these values the result will be a rendering
    // failure.
    const unsigned int kRayPayloadSize   = 14 * sizeof(float);
    const unsigned int kIntersectionSize = 2 * sizeof(float);
    auto* pShaderConfigSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    pShaderConfigSubobject->Config(kRayPayloadSize, kIntersectionSize);

    // NOTE: The shader configuration is assumed to apply to all shaders, so it is *not* associated
    // with specific shaders (which would use
    // CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT).

    // Create the global root signature subobject.
    auto* pGlobalRootSignatureSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    pGlobalRootSignatureSubobject->SetRootSignature(_pGlobalRootSignature.Get());

    // Create the local root signature subobject associated with the ray generation shader (which is
    // compiled with the default builtin shader)
    auto* pRayGenRootSignatureSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    pRayGenRootSignatureSubobject->SetRootSignature(_pRayGenRootSignature.Get());
    auto* pAssociationSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    pAssociationSubobject->SetSubobjectToAssociate(*pRayGenRootSignatureSubobject);
    pAssociationSubobject->AddExport(kRayGenEntryPointName);

    // Create hit group use by all instances (required even if only has miss shader.)
    auto* pInstanceClosestHitRootSigSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    pInstanceClosestHitRootSigSubobject->SetRootSignature(_pInstanceHitRootSignature.Get());
    pAssociationSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    pAssociationSubobject->SetSubobjectToAssociate(*pInstanceClosestHitRootSigSubobject);
    pAssociationSubobject->AddExport(kInstanceClosestHitEntryPointName);

    auto* pInstanceShaderSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    pInstanceShaderSubobject->SetHitGroupExport(kInstanceHitGroupName);

    pInstanceShaderSubobject->SetClosestHitShaderImport(kInstanceClosestHitEntryPointName);
    pInstanceShaderSubobject->SetAnyHitShaderImport(kInstanceShadowAnyHitEntryPointName);
    pInstanceShaderSubobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    pAssociationSubobject->AddExport(kInstanceHitGroupName);

    // Keep track of number of active shaders.
    int activeShaders = 0;

    // Create a DXR hit group for each shader.
    for (int i = 0; i < _compiledShaders.size(); i++)
    {
        // Get the shader.
        MaterialShaderPtr pShader = _shaderLibrary.get(i);
        if (pShader)
        {
            // Increment active shader.
            activeShaders++;
        }
    }

    // Create the pipeline state object from the description.
    float plStart = _timer.elapsed();
    checkHR(_pDXDevice->CreateStateObject(pipelineStateDesc, IID_PPV_ARGS(&_pPipelineState)));
    float plEnd = _timer.elapsed();

    // Get the total time taken to rebuild.
    // TODO: This should go into a stats property set and exposed to client properly.
    float elapsedMillisec = _timer.elapsed();

    // Dump breakdown of rebuild timing.
    AU_INFO("Compiled %d shaders and linked %d in %d ms", compileJobs.size(), activeShaders,
        static_cast<int>(elapsedMillisec));
    AU_INFO("  - Slang session creation took %d ms", static_cast<int>(scEnd - scStart));
    AU_INFO("  - Transpilation and DXC compile took %d ms", static_cast<int>(compEnd - compStart));
    AU_INFO(
        "  - DXC link took %d ms (Hash:%llx)", static_cast<int>(linkEnd - linkStart), libraryHash);
    AU_INFO("  - Pipeline creation took %d ms", static_cast<int>(plEnd - plStart));
}

END_AURORA
