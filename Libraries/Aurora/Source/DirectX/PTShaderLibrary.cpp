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
#define AU_DEV_MULTITHREAD_COMPILATION 0

#if AU_DEV_MULTITHREAD_COMPILATION
#include <tbb/blocked_range.h>
#include <tbb/parallel_for_each.h>
#endif

BEGIN_AURORA

// Development flag to entire HLSL library to disk.
// NOTE: This should never be enabled in committed code; it is only for local development.
#define AU_DEV_DUMP_SHADER_CODE 0

// Pack four bytes into a single unsigned int.
// Based on version in DirectX toolkit.
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                             \
    (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) |                                            \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) |                                  \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) |                                 \
        (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))

// Strings for shader entry points.
static const wchar_t* gRayGenEntryPoint         = L"RayGenShader";
static const wchar_t* gBackgroundMissEntryPoint = L"BackgroundMissShader";
static const wchar_t* gRadianceMissEntryPoint   = L"RadianceMissShader";
static const wchar_t* gShadowMissEntryPoint     = L"ShadowMissShader";

// Combine the source code for the reference and standard surface BSDF to produce the default built
// in material type. Use USE_REFERENCE_BSDF ifdef so that the material's BSDF can be selected via an
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

PTMaterialType::PTMaterialType(
    PTShaderLibrary* pShaderLibrary, int sourceIndex, const string& typeName) :
    _pShaderLibrary(pShaderLibrary), _sourceIndex(sourceIndex), _name(typeName)
{
    // Set the closest hit, any hit, and layer miss hit entry point names, converted to wide string.
    _closestHitEntryPoint        = Foundation::s2w(typeName + "RadianceHitShader");
    _shadowAnyHitEntryPoint      = Foundation::s2w(typeName + "ShadowAnyHitShader");
    _materialLayerMissEntryPoint = Foundation::s2w(typeName + "LayerMissShader");

    // Set the hit group export name from the entry point name, converted to wide string.
    _exportName = _closestHitEntryPoint + Foundation::s2w("Group");

    // Initialize ref. counts to zero.
    for (int i = 0; i < EntryPoint::kNumEntryPoints; i++)
        _entryPointRefCount[i] = 0;
}

PTMaterialType::~PTMaterialType()
{
    // If this material type is valid, when its destroyed (which will happen when no material holds
    // a shared pointer to it) remove it source code from the library.
    if (isValid())
    {
        _pShaderLibrary->removeSource(_sourceIndex);
    }
}

void PTMaterialType::incrementRefCount(EntryPoint entryPoint)
{
    // Increment the ref. count and trigger rebuild if this is the flip case (that causes the count
    // to go from zero to non-zero.) A shader rebuild is required as this will change the HLSL code.
    _entryPointRefCount[entryPoint]++;
    if (_pShaderLibrary && _entryPointRefCount[entryPoint] == 1)
        _pShaderLibrary->triggerRebuild();
}

void PTMaterialType::decrementRefCount(EntryPoint entryPoint)
{
    // Ensure ref. count is non-zero.
    AU_ASSERT(_entryPointRefCount[entryPoint] > 0, "Invalid ref count");

    // Decrement the ref. count and trigger rebuild if this is the flip case (that causes the count
    // to go from non-zero to zero). A shader rebuild is required as this will change the HLSL code.
    _entryPointRefCount[entryPoint]--;
    if (_pShaderLibrary && _entryPointRefCount[entryPoint] == 0)
        _pShaderLibrary->triggerRebuild();
}

DirectXShaderIdentifier PTMaterialType::getShaderID()
{
    // Get the shader ID from the library.
    return _pShaderLibrary->getShaderID(_exportName.c_str());
}

DirectXShaderIdentifier PTMaterialType::getLayerShaderID()
{
    // Get the shader ID from the library.
    return _pShaderLibrary->getShaderID(_materialLayerMissEntryPoint.c_str());
}

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

PTShaderLibrary::~PTShaderLibrary()
{
    // Invalidate any remaining valid material types, to avoid zombies.
    for (auto pWeakMaterialType : _materialTypes)
    {
        PTMaterialTypePtr pMaterialType = pWeakMaterialType.second.lock();
        if (pMaterialType)
        {
            pMaterialType->invalidate();
        }
    }
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
    // which shaders can use by default, as well as dynamic samplers per-meterial.
    CD3DX12_DESCRIPTOR_RANGE texRange;
    CD3DX12_DESCRIPTOR_RANGE samplerRange;

    CD3DX12_ROOT_PARAMETER globalRootParameters[8] = {}; // NOLINT(modernize-avoid-c-arrays)
    globalRootParameters[0].InitAsShaderResourceView(0); // gScene: acceleration structure
    globalRootParameters[1].InitAsConstants(2, 0);       // sampleIndex + seedOffset
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
    CD3DX12_ROOT_SIGNATURE_DESC globalDesc(
        _countof(globalRootParameters), globalRootParameters, 1, &samplerDesc);
    _pGlobalRootSignature = createRootSignature(globalDesc);
    _pGlobalRootSignature->SetName(L"Global Root Signature");

    // Specify a local root signature for the ray gen shader.
    // NOTE: The output UAVs are typed, and therefore can't be used with a root descriptor; they
    // must come from a descriptor heap.
    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 7, 0);   // Output images (for AOV data)
    CD3DX12_DESCRIPTOR_RANGE rayGenRanges[] = { uavRange }; // NOLINT(modernize-avoid-c-arrays)
    CD3DX12_ROOT_PARAMETER rayGenRootParameters[1] = {};    // NOLINT(modernize-avoid-c-arrays)
    rayGenRootParameters[0].InitAsDescriptorTable(_countof(rayGenRanges), rayGenRanges);
    CD3DX12_ROOT_SIGNATURE_DESC rayGenDesc(_countof(rayGenRootParameters), rayGenRootParameters);
    rayGenDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pRayGenRootSignature = createRootSignature(rayGenDesc);
    _pRayGenRootSignature->SetName(L"Ray Gen Local Root Signature");

    // Specify a local root signature for the radiance hit group.
    // NOTE: All shaders in the hit group must have the same local root signature.
    CD3DX12_ROOT_PARAMETER radianceHitParameters[9] = {};    // NOLINT(modernize-avoid-c-arrays)
    radianceHitParameters[0].InitAsShaderResourceView(0, 1); // gIndices: indices
    radianceHitParameters[1].InitAsShaderResourceView(1, 1); // gPositions: positions
    radianceHitParameters[2].InitAsShaderResourceView(2, 1); // gNormals: normals
    radianceHitParameters[3].InitAsShaderResourceView(3, 1); // gTexCoords: texture coordinates
    radianceHitParameters[4].InitAsConstants(
        3, 0, 1); // gHasNormals, gHasTexCoords, and gLayerMissShaderIndex
    radianceHitParameters[5].InitAsConstantBufferView(1, 1); // gMaterial: material data
    radianceHitParameters[6].InitAsConstantBufferView(
        2, 1); // gMaterialLayerIDs: indices for layer material shaders

    // Texture descriptors starting at register(t4, space1)
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PTMaterial::descriptorCount(), 4, 1);
    CD3DX12_DESCRIPTOR_RANGE radianceHitRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    radianceHitParameters[7].InitAsDescriptorTable(_countof(radianceHitRanges), radianceHitRanges);

    // Sampler descriptors starting at register(s1)
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, PTMaterial::samplerDescriptorCount(), 1);
    CD3DX12_DESCRIPTOR_RANGE radianceHitSamplerRanges[] = {
        samplerRange
    }; // NOLINT(modernize-avoid-c-arrays)
    radianceHitParameters[8].InitAsDescriptorTable(
        _countof(radianceHitSamplerRanges), radianceHitSamplerRanges);

    CD3DX12_ROOT_SIGNATURE_DESC radianceHitDesc(
        _countof(radianceHitParameters), radianceHitParameters);
    radianceHitDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pRadianceHitRootSignature = createRootSignature(radianceHitDesc);
    _pRadianceHitRootSignature->SetName(L"Radiance Hit Group Local Root Signature");

    // Specify a local root signature for the layer miss shader.
    // NOTE: All shaders in the hit group must have the same local root signature.
    CD3DX12_ROOT_PARAMETER layerMissParameters[9] = {};    // NOLINT(modernize-avoid-c-arrays)
    layerMissParameters[0].InitAsShaderResourceView(0, 1); // gIndices: indices
    layerMissParameters[1].InitAsShaderResourceView(1, 1); // gPositions: positions
    layerMissParameters[2].InitAsShaderResourceView(2, 1); // gNormals: normals
    layerMissParameters[3].InitAsShaderResourceView(3, 1); // gTexCoords: texture coordinates
    layerMissParameters[4].InitAsConstants(
        3, 0, 1); // gHasNormals, gHasTexCoords, and gLayerMissShaderIndex
    layerMissParameters[5].InitAsConstantBufferView(1, 1); // gMaterial: material data
    layerMissParameters[6].InitAsConstantBufferView(
        2, 1); // gMaterialLayerIDs: indices for layer material shaders

    // Texture descriptors starting at register(t4, space1)
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PTMaterial::descriptorCount(), 4, 1); // Textures
    CD3DX12_DESCRIPTOR_RANGE layerMissRanges[] = { texRange }; // NOLINT(modernize-avoid-c-arrays)
    layerMissParameters[7].InitAsDescriptorTable(_countof(layerMissRanges), layerMissRanges);

    // Sampler descriptors starting at register(s1)
    samplerRange.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, PTMaterial::samplerDescriptorCount(), 1); // Samplers
    CD3DX12_DESCRIPTOR_RANGE layerMissSamplerRanges[] = {
        samplerRange
    }; // NOLINT(modernize-avoid-c-arrays)
    layerMissParameters[8].InitAsDescriptorTable(
        _countof(layerMissSamplerRanges), layerMissSamplerRanges);

    // Create layer miss root signature.
    CD3DX12_ROOT_SIGNATURE_DESC layerMissDesc(_countof(layerMissParameters), layerMissParameters);
    layerMissDesc.Flags      = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _pLayerMissRootSignature = createRootSignature(layerMissDesc);
    _pLayerMissRootSignature->SetName(L"Layer Miss Local Root Signature");
}

DirectXShaderIdentifier PTShaderLibrary::getShaderID(const wchar_t* entryPoint)
{
    // Assert if a rebuild is required, as the pipeline state will be invalid.
    AU_ASSERT(!_rebuildRequired,
        "Shader Library rebuild required, call rebuild() before accessing shaders");

    // Get the shader ID from the pipeline state.
    ID3D12StateObjectPropertiesPtr stateObjectProps;
    _pPipelineState.As(&stateObjectProps);
    return stateObjectProps->GetShaderIdentifier(entryPoint);
}

DirectXShaderIdentifier PTShaderLibrary::getBackgroundMissShaderID()
{
    // Get the shader ID for the HLSL function name.
    return getShaderID(gBackgroundMissEntryPoint);
}

DirectXShaderIdentifier PTShaderLibrary::getRadianceMissShaderID()
{
    // Get the shader ID for the HLSL function name.
    return getShaderID(gRadianceMissEntryPoint);
}

DirectXShaderIdentifier PTShaderLibrary::getShadowMissShaderID()
{
    // Get the shader ID for the HLSL function name.
    return getShaderID(gShadowMissEntryPoint);
}

DirectXShaderIdentifier PTShaderLibrary::getRayGenShaderID()
{
    // Get the shader ID for the HLSL function name.
    return getShaderID(gRayGenEntryPoint);
}

vector<string> PTShaderLibrary::getActiveTypeNames()
{
    vector<string> res;
    for (auto hgIter = _materialTypes.begin(); hgIter != _materialTypes.end(); hgIter++)
    {
        // Weak pointer is stored in shader library, ensure it has not been deleted.
        PTMaterialTypePtr pMtlType = hgIter->second.lock();
        if (pMtlType)
        {
            res.push_back(pMtlType->name());
        }
    }
    return res;
}

PTMaterialTypePtr PTShaderLibrary::getType(const string& name)
{
    return _materialTypes[name].lock();
}

void PTShaderLibrary::removeSource(int sourceIndex)
{
    // Push index in to vector, the actual remove only happens when library rebuilt.
    _sourceToRemove.push_back(sourceIndex);
}
PTMaterialTypePtr PTShaderLibrary::acquireMaterialType(
    const MaterialTypeSource& source, bool* pCreatedType)
{
    // The shared pointer to material type.
    PTMaterialTypePtr pMtlType;

    // First see if this entry point already exists.
    map<string, weak_ptr<PTMaterialType>>::iterator hgIter = _materialTypes.find(source.name);
    if (hgIter != _materialTypes.end())
    {
        // Weak pointer is stored in shader library, ensure it has not been deleted.
        pMtlType = hgIter->second.lock();
        if (pMtlType)
        {
            // If the entry point exists, in debug mode do a string comparison to ensure the source
            // also matches.
            AU_ASSERT_DEBUG(
                source.compareSource(_compiledMaterialTypes[pMtlType->_sourceIndex].source),
                "Source mis-match for material type %s.", source.name.c_str());

            // No type created.
            if (pCreatedType)
                *pCreatedType = false;

            // Return the existing material type.
            return pMtlType;
        }
    }

    // Append the new source to the source vector, and calculate source index.
    int sourceIdx = static_cast<int>(_compiledMaterialTypes.size());
    _compiledMaterialTypes.push_back({ source, nullptr });

    // Trigger rebuild.
    _rebuildRequired = true;

    // Create new material type.
    pMtlType = make_shared<PTMaterialType>(this, sourceIdx, source.name);

    // Add weak reference to map.
    _materialTypes[source.name] = weak_ptr<PTMaterialType>(pMtlType);

    // New type created.
    if (pCreatedType)
        *pCreatedType = true;

    // Return the new material type.
    return pMtlType;
}

PTMaterialTypePtr PTShaderLibrary::getBuiltInMaterialType(const string& name)
{
    return _builtInMaterialTypes[name];
}

void PTShaderLibrary::initialize()
{
    _pTranspiler = make_shared<Transpiler>(CommonShaders::g_sDirectory);

    // Initialize root signatures (these are shared by all material types, and don't change.)
    initRootSignatures();

    // Clear the source and built ins vector. Not strictly needed, but this function could be called
    // repeatedly in the future.
    _compiledMaterialTypes.clear();
    _builtInMaterialNames = {};

    // Create the default material type.
    MaterialTypeSource defaultMaterialSource(
        "Default", CommonShaders::g_sInitializeDefaultMaterialType);
    PTMaterialTypePtr pDefaultMaterialType = acquireMaterialType(defaultMaterialSource);

    // Ensure the radiance hit entry point is compiled for default material type.
    pDefaultMaterialType->incrementRefCount(PTMaterialType::EntryPoint::kRadianceHit);

    // Add default material type to the built-in array.
    _builtInMaterialNames.push_back(defaultMaterialSource.name);
    _builtInMaterialTypes[defaultMaterialSource.name] =
        pDefaultMaterialType; // Stores strong reference to the built-in's material type.
}

bool PTShaderLibrary::setDefinitionsHLSL(const string& definitions)
{
    // If the MaterialX definitions HLSL has not changed do nothing.
    if (_materialXDefinitionsSource.compare(definitions) == 0)
    {
        return false;
    }

    // Otherwise set definitions and trigger rebuild.
    _materialXDefinitionsSource = definitions;
    _rebuildRequired            = true;
    return true;
}

bool PTShaderLibrary::setOption(const string& name, int value)
{
    // Set the option and see if actually changed.
    bool changed = _options.set(name, value);

    // If the option changed set the HLSL and trigger rebuild.
    if (changed)
    {
        _optionsSource   = _options.toHLSL();
        _rebuildRequired = true;
    }

    return changed;
}

void PTShaderLibrary::assembleShadersForMaterialType(const MaterialTypeSource& source,
    const map<string, bool>& entryPoints, vector<string>& shadersOut)
{
    // Add shared common code.
    string shaderSource = _optionsSource;
    shaderSource += CommonShaders::g_sGLSLToHLSL;
    shaderSource += CommonShaders::g_sMaterialXCommon;
    shaderSource += _materialXDefinitionsSource;

    // Clear the output shaders vector.
    shadersOut.clear();

    // Add the shaders for the radiance hit entry point, if needed.
    if (entryPoints.at("RADIANCE_HIT"))
    {

        // Create radiance hit entry point, by replacing template tags with the material type name.
        string radianceHitEntryPointSource = regex_replace(
            CommonShaders::g_sClosestHitEntryPointTemplate, regex("@MATERIAL_TYPE@"), source.name);
        shadersOut.push_back(shaderSource + radianceHitEntryPointSource);

        // Create shadow hit entry point, by replacing template tags with the material type name.
        string shadowHitEntryPointSource = regex_replace(
            CommonShaders::g_sShadowHitEntryPointTemplate, regex("@MATERIAL_TYPE@"), source.name);
        shadersOut.push_back(shaderSource + shadowHitEntryPointSource);
    }

    // Add the shaders for the layer miss entry point, if needed.
    if (entryPoints.at("LAYER_MISS"))
    {
        // Create layer miss entry point, by replacing template tags with the material type name.
        string layerMissShaderSource = regex_replace(
            CommonShaders::g_sLayerShaderEntryPointTemplate, regex("@MATERIAL_TYPE@"), source.name);
        shadersOut.push_back(shaderSource + layerMissShaderSource);
    }
}

// Input to thread used to compile shaders.
struct CompileJob
{
    string code;
    string libName;
    ComPtr<IDxcBlob> pBlob;
    map<string, string> includes;
};

void PTShaderLibrary::rebuild()
{
    // Start timer.
    _timer.reset();

    // Should only be called if required (rebuilding requires stalling the GPU pipeline.)
    AU_ASSERT(_rebuildRequired, "Rebuild not needed");

    // This creates the following subjects for a pipeline state object:
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
    auto* pPipelineConfigSuboject =
        pipelineStateDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pPipelineConfigSuboject->Config(PTRenderer::kMaxTraceDepth + 1);

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

    // Clear any source that has been removed.
    // TODO: Allow re-use of source indices.
    for (auto sourceIndex : _sourceToRemove)
    {
        _compiledMaterialTypes[sourceIndex].reset();
    }
    _sourceToRemove.clear();

    // Build vector of compile jobs to execute in paralell.
    vector<CompileJob> compileJobs;

    // Add code to be compiled for the common shared entry points.
    // TODO: Just compile once at the start of day.
    compileJobs.push_back({ CommonShaders::g_sBackgroundMissShader, "BackgroundMiss", nullptr });
    compileJobs.push_back({ CommonShaders::g_sRadianceMissShader, "RadianceMiss", nullptr });
    compileJobs.push_back({ CommonShaders::g_sShadowMissShader, "ShadowMiss", nullptr });
    compileJobs.push_back({ CommonShaders::g_sRayGenShader, "RayGen", nullptr });

    // Assemble the shader code for for each material type.
    for (int i = 0; i < _compiledMaterialTypes.size(); i++)
    {
        auto& compiledMtlType = _compiledMaterialTypes[i];
        if (!compiledMtlType.source.empty() && !compiledMtlType.binary)
        {
            // Get the material type.
            PTMaterialType* pMtlType = _materialTypes[compiledMtlType.source.name].lock().get();

            // Get entry points for material type (this may have changed since last rebuild)
            map<string, bool> entryPoints;
            pMtlType->getEntryPoints(entryPoints);

            // Assemble the shaders for all the entry points.
            vector<string> shaderCode;
            assembleShadersForMaterialType(compiledMtlType.source, entryPoints, shaderCode);

            // Add a compile job for each entry point.
            for (int j = 0; j < shaderCode.size(); j++)
            {
                // Add to compile jobs to be built, adding the material type source as includes map.
                compileJobs.push_back({ shaderCode[j], compiledMtlType.source.name + to_string(j),
                    nullptr, { { "InitializeMaterial.slang", compiledMtlType.source.setup } } });
            }
        }
    }

    // Compile function called from parallel thread.
    auto compileFunc = [this](CompileJob& job) {
        // If development flag set dump HLSL library to a file.
        if (AU_DEV_DUMP_SHADER_CODE)
        {
            string entryPointPath = Foundation::getModulePath() + job.libName + ".txt";
            AU_INFO("Dumping shader code to:%s", entryPointPath.c_str());
            ofstream outputFile;
            outputFile.open(entryPointPath);
            outputFile << job.code;
            outputFile.close();
        }

        // Set the material type source as source code available as via #include in the compiler.
        for (auto iter = job.includes.begin(); iter != job.includes.end(); iter++)
        {
            _pTranspiler->setSource(iter->first, iter->second);
        }

        // Transpile the source.
        string transpiledHLSL;
        string transpilerErrors;
        if (!_pTranspiler->transpileCode(
                job.code, transpiledHLSL, transpilerErrors, Transpiler::Language::HLSL))
        {
            AU_ERROR("Slang transpiling error log:\n%s", transpilerErrors.c_str());
            AU_DEBUG_BREAK();
            AU_FAIL("Slang transpiling failed, see log in console for details.");
        }

        // Compile the HLSL source containing all the material types.
        ComPtr<IDxcBlob> compiledShader;
        vector<pair<wstring, string>> defines = { { L"DIRECTX", "1" } };
        string errorMessage;
        if (!compileLibrary(_pDXCLibrary,
                transpiledHLSL.c_str(), // Source code string.
                "AuroraShaderLibrary",  // Arbitrary shader name
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
            AU_FAIL("HLSL compilation failed, see log in console for details.");
        }
        job.pBlob = compiledShader;
    };

    // Compile all the material types.
    vector<ComPtr<IDxcBlob>> compiledShaders;
    vector<string> compiledShaderNames;
    float compStart = _timer.elapsed();
#if AU_DEV_MULTITHREAD_COMPILATION // Set to 1 to force single threaded.
    tbb::parallel_for(tbb::blocked_range<int>(0, (int)compileJobs.size()),
        [&compileJobs, compileFunc](tbb::blocked_range<int> r) {
            for (int i = r.begin(); i < r.end(); ++i)
            {
                compileFunc(compileJobs[i]);
            }
        });
#else
    for (auto& job : compileJobs)
    {
        compileFunc(job);
    }
#endif

    float compEnd = _timer.elapsed();

    // Build array of shader binaries and names for linking.
    for (auto& job : compileJobs)
    {
        compiledShaders.push_back(job.pBlob);
        compiledShaderNames.push_back(job.libName);
    }

    // Link the compiled shaders into single library.
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
    auto* pGlobalRootSignatureSuboject =
        pipelineStateDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    pGlobalRootSignatureSuboject->SetRootSignature(_pGlobalRootSignature.Get());

    // Create the local root signature subobject associated with the ray generation shader.
    auto* pRayGenRootSignatureSuboject =
        pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    pRayGenRootSignatureSuboject->SetRootSignature(_pRayGenRootSignature.Get());
    auto* pAssociationSubobject =
        pipelineStateDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    pAssociationSubobject->SetSubobjectToAssociate(*pRayGenRootSignatureSuboject);
    pAssociationSubobject->AddExport(gRayGenEntryPoint);

    // Keep track of number of active material types.
    int activeMaterialTypes = 0;

    // Create a DXR hit group for each material type.
    for (auto pWeakMaterialType : _materialTypes)
    {
        PTMaterialTypePtr pMaterialType = pWeakMaterialType.second.lock();
        if (pMaterialType)
        {

            if (pMaterialType->refCount(PTMaterialType::EntryPoint::kRadianceHit) == 0 &&
                pMaterialType->refCount(PTMaterialType::EntryPoint::kLayerMiss) == 0)
            {
                AU_WARN("Invalid material type %s: all entry point reference counts are zero!",
                    pMaterialType->name().c_str());
            }

            // Increment active material type.
            activeMaterialTypes++;

            // Create the local root signature subobject associated with the hit group.
            // All material types are based on the same radiance hit root signature currently.
            auto* pRadianceHitRootSignatureSuboject =
                pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            pRadianceHitRootSignatureSuboject->SetRootSignature(_pRadianceHitRootSignature.Get());
            pAssociationSubobject =
                pipelineStateDesc
                    .CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            pAssociationSubobject->SetSubobjectToAssociate(*pRadianceHitRootSignatureSuboject);

            // Create the radiance hit group subobject, which aggregates closest hit, any hit, and
            // intersection shaders as a group. In this case, there is a closest hit shader for
            // radiance rays, and an any hit shader for shadow rays.
            // NOTE: Normally a hit group corresponds to a single ray type, but here we are able to
            // combine code for both radiance and shadow rays into a single hit group. If we needed
            // an any hit shader for radiance rays, then a separate hit group for shadow rays would
            // have to be created, and referenced with an offset in the related TraceRay() calls.

            // Create hit group (required even if only has miss shader.)
            auto* pMaterialTypeSuboject =
                pipelineStateDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            pMaterialTypeSuboject->SetHitGroupExport(pMaterialType->exportName().c_str());
            pAssociationSubobject->AddExport(pMaterialType->exportName().c_str());

            if (pMaterialType->refCount(PTMaterialType::EntryPoint::kRadianceHit) > 0)
            {

                pMaterialTypeSuboject->SetClosestHitShaderImport(
                    pMaterialType->closestHitEntryPoint().c_str());
                pMaterialTypeSuboject->SetAnyHitShaderImport(
                    pMaterialType->shadowAnyHitEntryPoint().c_str());
                pMaterialTypeSuboject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

                pAssociationSubobject->AddExport(pMaterialType->exportName().c_str());
            }

            if (pMaterialType->refCount(PTMaterialType::EntryPoint::kLayerMiss) > 0)
            {

                auto* pLayerMissRootSignatureSuboject =
                    pipelineStateDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
                pLayerMissRootSignatureSuboject->SetRootSignature(_pLayerMissRootSignature.Get());
                pAssociationSubobject =
                    pipelineStateDesc
                        .CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
                pAssociationSubobject->SetSubobjectToAssociate(*pLayerMissRootSignatureSuboject);
                pAssociationSubobject->AddExport(
                    pMaterialType->materialLayerMissEntryPoint().c_str());
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

    // Rebuild is no longer required.
    _rebuildRequired = false;

    // Get time taken to rebuild.
    // TODO: This should go into a stats property set and exposed to client properly.
    float elapsedMillisec = _timer.elapsed();

    AU_INFO("Compiled %d material types in %d ms", activeMaterialTypes,
        static_cast<int>(elapsedMillisec));
    AU_INFO("  - DXC compile took %d ms", static_cast<int>(compEnd - compStart));
    AU_INFO("  - DXC link took %d ms", static_cast<int>(linkEnd - linkStart));
    AU_INFO("  - Pipeline creation took %d ms", static_cast<int>(plEnd - plStart));
}

END_AURORA
