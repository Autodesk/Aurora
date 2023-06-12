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
#include "PTGeometry.h"
#include "PTImage.h"
#include "PTMaterial.h"
#include "PTRenderer.h"
#include "PTScene.h"
#include "PTShaderLibrary.h"
#include "PTTarget.h"
#include "Transpiler.h"

// Development flag to enable/disable multithreaded compilation.
#define AU_DEV_MULTITHREAD_COMPILATION 1

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
const string EntryPointTypes::kRadianceHit    = "RADIANCE_HIT";
const string EntryPointTypes::kLayerMiss      = "LAYER_MISS";
const string EntryPointTypes::kShadowAnyHit   = "SHADOW_ANY_HIT";
const string EntryPointTypes::kRayGen         = "RAY_GEN";
const string EntryPointTypes::kBackgroundMiss = "BACKGROUND_MISS";
const string EntryPointTypes::kRadianceMiss   = "RADIANCE_MISS";
const string EntryPointTypes::kShadowMiss     = "SHADOW_MISS";

// Array of entry point names.
const vector<string> PTShaderLibrary::DefaultEntryPoints = { EntryPointTypes::kRadianceHit,
    EntryPointTypes::kShadowAnyHit, EntryPointTypes::kLayerMiss };

// Combine the source code for the reference and Standard Surface BSDF to produce the default built
// in shader. Use USE_REFERENCE_BSDF ifdef so that the material's BSDF can be selected via an
// option.

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
        hlslStr += "#define " + _data[i].first + "  " + to_string(_data[i].second) + "\n";
    }

    // Return HLSL.
    return hlslStr;
}

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

void PTShaderLibrary::initRootSignatures()
{
    // Specify the global root signature for all shaders. This includes a global static sampler
    // which shaders can use by default, as well as dynamic samplers per-material.
    CD3DX12_DESCRIPTOR_RANGE texRange;
    CD3DX12_DESCRIPTOR_RANGE samplerRange;

    array<CD3DX12_ROOT_PARAMETER, 8> globalRootParameters = {}; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[0].InitAsShaderResourceView(0);        // gScene: acceleration structure
    globalRootParameters[1].InitAsConstants(2, 0);              // sampleIndex + seedOffset
    globalRootParameters[2].InitAsConstantBufferView(1); // gFrameData: per-frame constant buffer
    globalRootParameters[3].InitAsConstantBufferView(2); // gEnvironmentConstants
    globalRootParameters[4].InitAsShaderResourceView(1); // gEnvironmentAliasMap
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2,
        2); // gEnvironmentLight/BackgroundTexture (starting at t2)
    CD3DX12_DESCRIPTOR_RANGE globalRootRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[5].InitAsDescriptorTable(_countof(globalRootRanges), globalRootRanges);
    globalRootParameters[6].InitAsConstantBufferView(3); // gGroundPlane
    globalRootParameters[7].InitAsShaderResourceView(4); // gNullScene: null acceleration structure
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
    CD3DX12_ROOT_SIGNATURE_DESC globalDesc(static_cast<UINT>(globalRootParameters.size()),
        globalRootParameters.data(), 1, &samplerDesc);
    _pGlobalRootSignature = createRootSignature(globalDesc);
    _pGlobalRootSignature->SetName(L"Global Root Signature");

    // Specify a local root signature for the ray gen shader.
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

    // Specify a local root signature for the radiance hit group.
    // NOTE: All shaders in the hit group must have the same local root signature.
    array<CD3DX12_ROOT_PARAMETER, 10> radianceHitParameters = {};
    radianceHitParameters[0].InitAsShaderResourceView(0, 1); // gIndices: indices
    radianceHitParameters[1].InitAsShaderResourceView(1, 1); // gPositions: positions
    radianceHitParameters[2].InitAsShaderResourceView(2, 1); // gNormals: normals
    radianceHitParameters[3].InitAsShaderResourceView(3, 1); // gTangents: tangents
    radianceHitParameters[4].InitAsShaderResourceView(4, 1); // gTexCoords: texture coordinates
    radianceHitParameters[5].InitAsConstants(
        5, 0, 1); // gHasNormals, gHasTangents gHasTexCoords, gLayerMissShaderIndex and gIsOpaque
    radianceHitParameters[6].InitAsShaderResourceView(
        9, 1); // gMaterialConstants: material data (stored in register 9 after textures)
    radianceHitParameters[7].InitAsConstantBufferView(
        2, 1); // gMaterialLayerIDs: indices for layer material shaders

    // Texture descriptors starting at register(t5, space1), after the byte address buffers for
    // vertex data.
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PTMaterial::descriptorCount(), 5, 1);
    CD3DX12_DESCRIPTOR_RANGE radianceHitRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    radianceHitParameters[8].InitAsDescriptorTable(_countof(radianceHitRanges), radianceHitRanges);

    // Sampler descriptors starting at register(s1)
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, PTMaterial::samplerDescriptorCount(), 1);
    CD3DX12_DESCRIPTOR_RANGE radianceHitSamplerRanges[] = {
        samplerRange
    }; // NOLINT(modernize-avoid-c-arrays)
    radianceHitParameters[9].InitAsDescriptorTable(
        _countof(radianceHitSamplerRanges), radianceHitSamplerRanges);

    CD3DX12_ROOT_SIGNATURE_DESC radianceHitDesc(
        static_cast<UINT>(radianceHitParameters.size()), radianceHitParameters.data());
    radianceHitDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pRadianceHitRootSignature = createRootSignature(radianceHitDesc);
    _pRadianceHitRootSignature->SetName(L"Radiance Hit Group Local Root Signature");

    // Specify a local root signature for the layer miss shader.
    // NOTE: All shaders in the hit group must have the same local root signature.
    array<CD3DX12_ROOT_PARAMETER, 10> layerMissParameters = {};
    layerMissParameters[0].InitAsShaderResourceView(0, 1); // gIndices: indices
    layerMissParameters[1].InitAsShaderResourceView(1, 1); // gPositions: positions
    layerMissParameters[2].InitAsShaderResourceView(2, 1); // gNormals: normals
    layerMissParameters[3].InitAsShaderResourceView(3, 1); // gTangents: tangents
    layerMissParameters[4].InitAsShaderResourceView(4, 1); // gTexCoords: texture coordinates
    layerMissParameters[5].InitAsConstants(
        5, 0, 1); // gHasNormals, gHasTangents, gHasTexCoords, gIsOpaque,and gLayerMissShaderIndex
    layerMissParameters[6].InitAsShaderResourceView(
        9, 1); // gMaterialConstants: material data (stored in register 9 after textures)
    layerMissParameters[7].InitAsConstantBufferView(
        2, 1); // gMaterialLayerIDs: indices for layer material shaders

    // Texture descriptors starting at register(t5, space1), after the byte address buffers for
    // vertex data.
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PTMaterial::descriptorCount(), 5, 1); // Textures
    CD3DX12_DESCRIPTOR_RANGE layerMissRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    layerMissParameters[8].InitAsDescriptorTable(_countof(layerMissRanges), layerMissRanges);

    // Sampler descriptors starting at register(s1)
    samplerRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, PTMaterial::samplerDescriptorCount(), 1); // Samplers
    CD3DX12_DESCRIPTOR_RANGE layerMissSamplerRanges[] = {
        samplerRange
    }; // NOLINT(modernize-avoid-c-arrays)
    layerMissParameters[9].InitAsDescriptorTable(
        _countof(layerMissSamplerRanges), layerMissSamplerRanges);

    // Create layer miss root signature.
    CD3DX12_ROOT_SIGNATURE_DESC layerMissDesc(
        static_cast<UINT>(layerMissParameters.size()), layerMissParameters.data());
    layerMissDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pLayerMissRootSignature = createRootSignature(layerMissDesc);
    _pLayerMissRootSignature->SetName(L"Layer Miss Local Root Signature");
}

DirectXShaderIdentifier PTShaderLibrary::getShaderID(const wchar_t* entryPoint)
{
    // Assert if a rebuild is required, as the pipeline state will be invalid.
    AU_ASSERT(!rebuildRequired(),
        "Shader Library rebuild required, call rebuild() before accessing shaders");

    // Get the shader ID from the pipeline state.
    ID3D12StateObjectPropertiesPtr stateObjectProps;
    _pPipelineState.As(&stateObjectProps);
    return stateObjectProps->GetShaderIdentifier(entryPoint);
}

DirectXShaderIdentifier PTShaderLibrary::getShaderID(MaterialShaderPtr pShader)
{
    auto& compiledShader = _compiledShaders[pShader->libraryIndex()];

    return getShaderID(Foundation::s2w(compiledShader.exportName).c_str());
}

DirectXShaderIdentifier PTShaderLibrary::getLayerShaderID(MaterialShaderPtr pShader)
{
    auto& compiledShader = _compiledShaders[pShader->libraryIndex()];

    return getShaderID(
        Foundation::s2w(compiledShader.entryPoints[EntryPointTypes::kLayerMiss]).c_str());
}

DirectXShaderIdentifier PTShaderLibrary::getSharedEntryPointShaderID(const string& entryPoint)
{
    // Get the default compiled shader, which contains all the shared entry points.
    const auto& defaultCompiledShader = getDefaultShader();

    // Get the shader ID for entry point.
    return getShaderID(Foundation::s2w(defaultCompiledShader.entryPoints.at(entryPoint)).c_str());
}

void PTShaderLibrary::initialize()
{
    // Create an emptry array of Slang transpilers.
    _transpilerArray = {};

    // Initialize root signatures (these are shared by all shaders, and don't change.)
    initRootSignatures();

    // Clear the source and built ins vector. Not strictly needed, but this function could be called
    // repeatedly in the future.
    _compiledShaders.clear();
    _builtInMaterialNames = {};

    // Create source code for the default shader.
    MaterialShaderSource defaultMaterialSource(
        "Default", CommonShaders::g_sInitializeDefaultMaterialType);

    // Add the shared entry points to the default shader's definitions source.
    defaultMaterialSource.definitions = "#include \"BackgroundMissShader.slang\"\n";
    defaultMaterialSource.definitions += "#include \"RadianceMissShader.slang\"\n";
    defaultMaterialSource.definitions += "#include \"ShadowMissShader.slang\"\n";
    defaultMaterialSource.definitions += "#include \"RayGenShader.slang\"\n";

    // Create the material definition for default shader.
    _builtInMaterialDefinitions[defaultMaterialSource.uniqueId] =
        make_shared<MaterialDefinition>(defaultMaterialSource,
            MaterialBase::StandardSurfaceDefaults, MaterialBase::updateBuiltInMaterial, false);

    // Create shader from the definition.
    MaterialShaderDefinition shaderDef;
    _builtInMaterialDefinitions[defaultMaterialSource.uniqueId]->getShaderDefinition(shaderDef);
    MaterialShaderPtr pDefaultShader = _shaderLibrary.acquire(shaderDef);

    // Ensure the radiance hit entry point is compiled for default shader.
    pDefaultShader->incrementRefCount(EntryPointTypes::kRadianceHit);

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

    // Setup the preprocessor defines to enable the entry points in the source code, based on shader
    // ref-counts.
    jobOut.code = "#define RADIANCE_HIT " +
        to_string(shader.hasEntryPoint(EntryPointTypes::kRadianceHit)) + "\n";
    jobOut.code += "#define LAYER_MISS " +
        to_string(shader.hasEntryPoint(EntryPointTypes::kLayerMiss)) + "\n\n";
    jobOut.code += "#define SHADOW_ANYHIT " +
        to_string(shader.hasEntryPoint(EntryPointTypes::kShadowAnyHit)) + "\n\n";
    jobOut.code += "#define SHADOW_ANYHIT_ALWAYS_OPAQUE " +
        to_string(shader.definition().isAlwaysOpaque) + "\n\n";

    // Create the shader entry points, by replacing template tags with the shader name.
    string entryPointSource =
        regex_replace(CommonShaders::g_sMainEntryPoints, regex("___Material___"), source.uniqueId);
    jobOut.code += entryPointSource;

    // Get the compiled shader for this material shader.
    auto& compiledShader = _compiledShaders[shader.libraryIndex()];

    // Set the library name to the shader name.
    jobOut.libName = compiledShader.hlslFilename;

    // Set the includes from the shader's source code and the options source.
    jobOut.includes = {
        { "InitializeMaterial.slang", source.setup },
        { "Options.slang", _optionsSource },
        { "Definitions.slang", source.definitions },
    };

    // Set the entry points to be post-processed (Slang will remove the [shader] tags).
    jobOut.entryPoints = { { "closesthit",
                               compiledShader.entryPoints[EntryPointTypes::kRadianceHit] },
        { "anyhit", compiledShader.entryPoints[EntryPointTypes::kShadowAnyHit] },
        { "miss", compiledShader.entryPoints[EntryPointTypes::kLayerMiss] } };

    // Set the index to map back to the compiled shader array.
    jobOut.index = shader.libraryIndex();
}

void PTShaderLibrary::rebuild()
{
    // Start timer.
    _timer.reset();

    // Should only be called if required (rebuilding requires stalling the GPU pipeline.)
    AU_ASSERT(rebuildRequired(), "Rebuild not needed");

    // Build vector of compile jobs to execute in parallel.
    vector<CompileJob> compileJobs;

    // Compile function is executed by MaterialShaderLibrary::update for any shaders that need
    // recompiling.
    auto compileShaderFunction = [this, &compileJobs](const MaterialShader& shader) {
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
            // Create entry points
            _compiledShaders[shader.libraryIndex()].entryPoints = {
                { EntryPointTypes::kRadianceHit, shader.id() + "RadianceHitShader" },
                { EntryPointTypes::kLayerMiss, shader.id() + "LayerMissShader" },
                { EntryPointTypes::kShadowAnyHit, shader.id() + "ShadowAnyHitShader" },
            };

            // Default shader (at index 0) has all the shared entry points.
            if (shader.libraryIndex() == 0)
            {
                _compiledShaders[shader.libraryIndex()]
                    .entryPoints[EntryPointTypes::kRadianceMiss] = "RadianceMissShader";
                _compiledShaders[shader.libraryIndex()].entryPoints[EntryPointTypes::kRayGen] =
                    "RayGenShader";
                _compiledShaders[shader.libraryIndex()].entryPoints[EntryPointTypes::kShadowMiss] =
                    "ShadowMissShader";
                _compiledShaders[shader.libraryIndex()]
                    .entryPoints[EntryPointTypes::kBackgroundMiss] = "BackgroundMissShader";
            }

            // Set the hit group export name from the entry point name.
            _compiledShaders[shader.libraryIndex()].exportName =
                _compiledShaders[shader.libraryIndex()].entryPoints[EntryPointTypes::kRadianceHit] +
                "Group";

            // Set the ID from the shader ID.
            _compiledShaders[shader.libraryIndex()].id           = shader.id();
            _compiledShaders[shader.libraryIndex()].hlslFilename = shader.id() + ".hlsl";
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
        if (shader.libraryIndex() == 0)
        {
            compileJobs.back().entryPoints.push_back({ "miss", "BackgroundMissShader" });
            compileJobs.back().entryPoints.push_back({ "miss", "RadianceMissShader" });
            compileJobs.back().entryPoints.push_back({ "miss", "ShadowMissShader" });
            compileJobs.back().entryPoints.push_back({ "raygeneration", "RayGenShader" });
        }

        return true;
    };

    // Destroy function is executed by MaterialShaderLibrary::update for any shaders that need
    // releasing.
    auto destroyShaderFunction = [this](int index) { _compiledShaders[index].reset(); };

    // Run the MaterialShaderLibrary update function and return if shaders were compiled.
    if (!_shaderLibrary.update(compileShaderFunction, destroyShaderFunction))
        return;

    // If shaders were rebuild we must completely rebuild the pipeline state with the following
    // subjects:
    // - Pipeline configuration: the max trace recursion depth.
    // - DXIL library: the compiled shaders in a bundle.
    // - Shader configurations: the ray payload and intersection attributes sizes.
    // - Associations between shader configurations and specific shaders (NOT DONE HERE).
    // - The global root signature: inputs for all shaders.
    // - Any local root signatures: inputs for specific shaders.
    // - Associations between local root signatures and specific shaders.
    // - Hit groups: combinations of closest hit / any hit / intersection shaders.

    // Prepare an empty pipeline state object description.
    CD3DX12_STATE_OBJECT_DESC pipelineStateDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Create a pipeline configuration subobject, which simply specifies the recursion depth.
    // NOTE: Tracing beyond this depth leads to undefined behavior, e.g. incorrect rendering or
    // device removal. The shaders track the depth to ensure it is not exceeded. We allow one more
    // than the maximum to so that shadow rays can still be traced at the maximum trace depth.
    auto* pPipelineConfigSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pPipelineConfigSubobject->Config(PTRenderer::kMaxTraceDepth + 1);

    // Create the DXC library, linker, and compiler.
    // NOTE: DXCompiler.dll is set DELAYLOAD in the linker settings, so this will abort if DLL
    // not available.
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(_pDXCLibrary.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(_pDXCompiler.GetAddressOf()));
    DxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(_pDXLinker.GetAddressOf()));

    // Create a DXIL library subobject.
    // NOTE: Shaders are not subobjects, but the library containing them is a subobject. All of the
    // shader entry points are exported by default; if only specific entry points should be
    // exported, then DefineExport() on the subobject should be used.
    auto* pLibrarySubObject = pipelineStateDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

    // As the transpiler is not thread safe we must create one for each thread.
    // TODO: These are around 30-40Mb each so we could look at cleaning them up once a certain
    // number are allocated.
    for (size_t i = _transpilerArray.size(); i < compileJobs.size(); i++)
    {
        _transpilerArray.push_back(make_shared<Transpiler>(CommonShaders::g_sDirectory));
    }

    // Transpilation and DXC Compile function is called from parallel threads.
    auto compileFunc = [this](CompileJob& job) {
        // Get the transpiler for this thread
        auto pTranspiler = _transpilerArray[job.jobIndex];

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
        _compiledShaders[job.index].binary = compiledShader;
    };

    // Compile all the shaders in parallel (if AU_DEV_MULTITHREAD_COMPILATION is set.)
    float compStart = _timer.elapsed();
#if AU_DEV_MULTITHREAD_COMPILATION // Set to 0 to force single threaded.
    for_each(execution::par, compileJobs.begin(), compileJobs.end(),
        [compileFunc](CompileJob& job) { compileFunc(job); });
#else
    // Otherwise run in single thread.
    for (auto& job : compileJobs)
    {
        compileFunc(job);
    }
#endif
    float compEnd = _timer.elapsed();

    // Build array of all the shader binaries (not just the ones compiled this frame) and names for
    // linking.
    vector<ComPtr<IDxcBlob>> compiledShaders;
    vector<string> compiledShaderNames;
    for (int i = 0; i < _compiledShaders.size(); i++)
    {
        auto& compiledShader = _compiledShaders[i];
        if (compiledShader.binary)
        {
            compiledShaders.push_back(compiledShader.binary);
            string libName = compiledShader.id + ".hlsl";
            Foundation::sanitizeFileName(libName);
            compiledShaderNames.push_back(libName);
        }
    }

    // Link the compiled shaders into a single library blob.
    float linkStart = _timer.elapsed();
    ComPtr<IDxcBlob> linkedShader;
    string linkErrorMessage;
    if (!linkLibrary(compiledShaders, compiledShaderNames,
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

    // Create bytecode from compiled blob.
    D3D12_SHADER_BYTECODE shaderByteCode =
        CD3DX12_SHADER_BYTECODE(linkedShader->GetBufferPointer(), linkedShader->GetBufferSize());

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
    // (as defined in the shaders; see "RadianceRayPayload" and "LayerData") and intersection
    // attributes (UV barycentric coordinates).
    const unsigned int kRayPayloadSize   = 48 * sizeof(float);
    const unsigned int kIntersectionSize = 2 * sizeof(float);
    auto* pShaderConfigSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    pShaderConfigSubobject->Config(kRayPayloadSize, kIntersectionSize);

    // NOTE: The shader configuration is assumed to apply to all shaders, so it is *not* associated
    // with specific shaders (which would use CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT).

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
    pAssociationSubobject->AddExport(
        Foundation::s2w(getDefaultShader().entryPoints[EntryPointTypes::kRayGen]).c_str());

    // Keep track of number of active shaders.
    int activeShaders = 0;

    // Create a DXR hit group for each shader.
    for (int i = 0; i < _compiledShaders.size(); i++)
    {
        // Get the shader.
        MaterialShaderPtr pShader = _shaderLibrary.get(i);
        if (pShader)
        {
            // Get the compiled shader object for this shader.
            auto& compiledShader = _compiledShaders[i];

            // Ensure some of the hit points are active.
            if (!pShader->hasEntryPoint(EntryPointTypes::kRadianceHit) &&
                !pShader->hasEntryPoint(EntryPointTypes::kShadowAnyHit) &&
                !pShader->hasEntryPoint(EntryPointTypes::kLayerMiss))
            {
                AU_WARN("Invalid shader %s: all entry point reference counts are zero!",
                    pShader->id().c_str());
            }

            // Increment active shader.
            activeShaders++;

            // Create the local root signature subobject associated with the hit group.
            // All shaders are based on the same radiance hit root signature currently.
            auto* pRadianceHitRootSignatureSubobject =
                pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            pRadianceHitRootSignatureSubobject->SetRootSignature(_pRadianceHitRootSignature.Get());
            pAssociationSubobject =
                pipelineStateDesc
                    .CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            pAssociationSubobject->SetSubobjectToAssociate(*pRadianceHitRootSignatureSubobject);

            // Create the radiance hit group subobject, which aggregates closest hit, any hit, and
            // intersection shaders as a group. In this case, there is a closest hit shader for
            // radiance rays, and an any hit shader for shadow rays.
            // NOTE: Normally a hit group corresponds to a single ray type, but here we are able to
            // combine code for both radiance and shadow rays into a single hit group. If we needed
            // an any hit shader for radiance rays, then a separate hit group for shadow rays would
            // have to be created, and referenced with an offset in the related TraceRay() calls.

            // Create hit group (required even if only  has miss shader.)
            auto* pShaderSubobject =
                pipelineStateDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            pShaderSubobject->SetHitGroupExport(Foundation::s2w(compiledShader.exportName).c_str());
            pAssociationSubobject->AddExport(Foundation::s2w(compiledShader.exportName).c_str());

            // Setup the ClosestHitShaderImport (for radiance hit entry point) and, if needed,
            // AnyHitShaderImport (for shadow hit entry point) on the hit group if
            // the radiance hit reference count is non-zero.
            if (pShader->refCount(EntryPointTypes::kRadianceHit) > 0)
            {
                pShaderSubobject->SetClosestHitShaderImport(
                    Foundation::s2w(compiledShader.entryPoints[EntryPointTypes::kRadianceHit])
                        .c_str());
                // Only add the shadow anyhit sub-object if needed (as indicated by refcount)
                if (pShader->refCount(EntryPointTypes::kShadowAnyHit) > 0)
                {
                    pShaderSubobject->SetAnyHitShaderImport(
                        Foundation::s2w(compiledShader.entryPoints[EntryPointTypes::kShadowAnyHit])
                            .c_str());
                }
                pShaderSubobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

                pAssociationSubobject->AddExport(
                    Foundation::s2w(compiledShader.exportName).c_str());
            }

            // Setup layer miss subobject is the layer miss reference count is non-zero.
            if (pShader->refCount(EntryPointTypes::kLayerMiss) > 0)
            {
                auto* pLayerMissRootSignatureSubobject =
                    pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
                pLayerMissRootSignatureSubobject->SetRootSignature(_pLayerMissRootSignature.Get());
                pAssociationSubobject =
                    pipelineStateDesc
                        .CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
                pAssociationSubobject->SetSubobjectToAssociate(*pLayerMissRootSignatureSubobject);
                pAssociationSubobject->AddExport(
                    Foundation::s2w(compiledShader.entryPoints[EntryPointTypes::kLayerMiss])
                        .c_str());
            }
        }
    }

    // Create the pipeline state object from the description.
    float plStart = _timer.elapsed();
    checkHR(_pDXDevice->CreateStateObject(pipelineStateDesc, IID_PPV_ARGS(&_pPipelineState)));
    float plEnd = _timer.elapsed();

    // Ensure GPU material structure matches CPU version.
    AU_ASSERT(
        PTMaterial::validateOffsets(*this), "Mismatch between GPU and CPU material structure");

    // Get the total time taken to rebuild.
    // TODO: This should go into a stats property set and exposed to client properly.
    float elapsedMillisec = _timer.elapsed();

    // Dump breakdown of rebuild timing.
    AU_INFO("Compiled %d shaders and linked %d in %d ms", compileJobs.size(), activeShaders,
        static_cast<int>(elapsedMillisec));
    AU_INFO("  - Transpilation and DXC compile took %d ms", static_cast<int>(compEnd - compStart));
    AU_INFO("  - DXC link took %d ms", static_cast<int>(linkEnd - linkStart));
    AU_INFO("  - Pipeline creation took %d ms", static_cast<int>(plEnd - plStart));
}

END_AURORA
