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

#include "PTScene.h"

#include "PTEnvironment.h"
#include "PTGeometry.h"
#include "PTGroundPlane.h"
#include "PTLight.h"
#include "PTMaterial.h"
#include "PTRenderer.h"
#include "PTShaderLibrary.h"

BEGIN_AURORA
#define kBuiltInMissShaderCount 4 // "built-in" miss shaders: null, background, radiance, and shadow

PTLayerIndexTable::PTLayerIndexTable(PTRenderer* pRenderer, const vector<int>& indices) :
    _pRenderer(pRenderer)
{
    if (!indices.empty())
        set(indices);
}

void PTLayerIndexTable::set(const vector<int>& indices)
{
    // Create a constant buffer for the material data if it doesn't already exist.
    static const size_t BUFFER_SIZE = sizeof(int) * kMaxMaterialLayers;
    if (!_pConstantBuffer)
    {
        _pConstantBuffer = _pRenderer->createBuffer(BUFFER_SIZE);
    }

    _count = static_cast<int>(indices.size());

    // Copy the indices to the constant buffer.
    int* pMappedData = nullptr;
    checkHR(_pConstantBuffer->Map(0, nullptr, (void**)&pMappedData));
    for (int i = 0; i < kMaxMaterialLayers; i++)
    {
        if (i < indices.size())
            pMappedData[i] = indices[i];
        else
            pMappedData[i] = -1;
    }
    _pConstantBuffer->Unmap(0, nullptr); // no HRESULT
}

// A structure that contains the properties for a hit group shader record, laid out for direct
// copying to a shader table buffer.
struct HitGroupShaderRecord
{
    // Constructor.
    HitGroupShaderRecord(const void* pShaderIdentifier, const PTGeometry::GeometryBuffers& geometry,
        const PTMaterial& material, ID3D12Resource* pMaterialLayerIndexBuffer,
        D3D12_GPU_DESCRIPTOR_HANDLE texturesHeapAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE samplersHeapAddress, int materialLayerCount)
    {
        ::memcpy_s(&ShaderIdentifier, SHADER_ID_SIZE, pShaderIdentifier, SHADER_ID_SIZE);
        IndexBufferAddress             = geometry.IndexBuffer;
        PositionBufferAddress          = geometry.PositionBuffer;
        HasNormals                     = geometry.NormalBuffer ? 1 : 0;
        NormalBufferAddress            = HasNormals ? geometry.NormalBuffer : 0;
        HasTangents                    = geometry.TangentBuffer != 0;
        TangentBufferAddress           = HasTangents ? geometry.TangentBuffer : 0;
        HasTexCoords                   = geometry.TexCoordBuffer != 0;
        TexCoordBufferAddress          = HasTexCoords ? geometry.TexCoordBuffer : 0;
        IsOpaque                       = material.isOpaque();
        MaterialLayerCount             = materialLayerCount;
        MaterialBufferAddress          = material.buffer()->GetGPUVirtualAddress();
        MaterialLayerIndexTableAddress = pMaterialLayerIndexBuffer
            ? pMaterialLayerIndexBuffer->GetGPUVirtualAddress()
            : (D3D12_GPU_VIRTUAL_ADDRESS) nullptr;
        TexturesHeapAddress            = texturesHeapAddress;
        SamplersHeapAddress            = samplersHeapAddress;
    }

    // Copies the contents of the shader record to the specified mapped buffer.
    void copyTo(void* mappedBuffer)
    {
        ::memcpy_s(mappedBuffer, sizeof(HitGroupShaderRecord), this, sizeof(HitGroupShaderRecord));
    }

    // Get the stride (aligned size) of a shader record.
    static size_t stride()
    {
        return ALIGNED_SIZE(sizeof(HitGroupShaderRecord), SHADER_RECORD_ALIGNMENT);
    }

    // These are the hit group arguments, in the order declared in the associated local root
    // signature. These must be aligned by their size, e.g. a root descriptor must be aligned on an
    // 8-byte (its size) boundary.
    array<uint8_t, SHADER_ID_SIZE> ShaderIdentifier;
    D3D12_GPU_VIRTUAL_ADDRESS IndexBufferAddress;
    D3D12_GPU_VIRTUAL_ADDRESS PositionBufferAddress;
    D3D12_GPU_VIRTUAL_ADDRESS NormalBufferAddress;
    D3D12_GPU_VIRTUAL_ADDRESS TangentBufferAddress;
    D3D12_GPU_VIRTUAL_ADDRESS TexCoordBufferAddress;
    uint32_t HasNormals;
    uint32_t HasTangents;
    uint32_t HasTexCoords;
    uint32_t MaterialLayerCount;
    uint32_t IsOpaque;
    D3D12_GPU_VIRTUAL_ADDRESS MaterialBufferAddress;
    D3D12_GPU_VIRTUAL_ADDRESS MaterialLayerIndexTableAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE TexturesHeapAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE SamplersHeapAddress;
};

PTInstance::PTInstance(PTScene* pScene, const PTGeometryPtr& pGeometry,
    const PTMaterialPtr& pMaterial, const mat4& transform, const LayerDefinitions& layers)
{
    AU_ASSERT(pGeometry, "Geometry assigned to an cannot be null.");

    // Set the arguments directly; they are assumed to be prepared by the caller.
    _pScene    = pScene;
    _pGeometry = pGeometry;
    _transform = transform;
    setMaterial(pMaterial);

    for (size_t i = 0; i < layers.size(); i++)
    {
        _layers.push_back(make_pair(dynamic_pointer_cast<PTMaterial>(layers[i].first),
            dynamic_pointer_cast<PTGeometry>(layers[i].second)));

        _layers[i].first->shader()->incrementRefCount(EntryPointTypes::kLayerMiss);
    }
}

PTInstance::~PTInstance()
{
    if (_pMaterial)
    {
        _pMaterial->shader()->decrementRefCount(EntryPointTypes::kRadianceHit);
        // The shadow anyhit is always attached.
        _pMaterial->shader()->decrementRefCount(EntryPointTypes::kShadowAnyHit);
    }

    for (size_t i = 0; i < _layers.size(); i++)
    {
        _layers[i].first->shader()->decrementRefCount(EntryPointTypes::kLayerMiss);
    }
}

void PTInstance::setMaterial(const IMaterialPtr& pMaterial)
{
    if (_pMaterial)
        _pMaterial->shader()->decrementRefCount(EntryPointTypes::kRadianceHit);

    // Cast the (optional) material to the renderer implementation. Use the default material if one
    // is / not specified.
    _pMaterial = pMaterial
        ? dynamic_pointer_cast<PTMaterial>(pMaterial)
        : dynamic_pointer_cast<PTMaterial>(_pScene->defaultMaterialResource()->resource());

    _pMaterial->shader()->incrementRefCount(EntryPointTypes::kRadianceHit);
    // The shadow anyhit is always attached. This is needed as the ordering is arbitrary for anyhit
    // shader invocations, so we cannot mix shaders with and without anyhit shadow shaders in the
    // same scene.
    _pMaterial->shader()->incrementRefCount(EntryPointTypes::kShadowAnyHit);

    // Set the instance as dirty.
    _bIsDirty = true;
}

void PTInstance::setTransform(const mat4& transform)
{
    // Create a GLM matrix from the (column-major) array if one is specified. Otherwise the
    // default / (identity) matrix is used.
    _transform = transform;

    // Set the instance as dirty.
    _bIsDirty = true;
}

void PTInstance::setObjectIdentifier(int /*objectId*/)
{
    // TODO: implement object id setting for Aurora
}

bool PTInstance::update()
{
    // Update the geometry and material.
    // NOTE: Whether these were dirty (i.e. return false) do not affect whether the instance was
    // considered dirty.
    _pMaterial->update();

    // Update the material and geometry for layer materials.
    for (size_t i = 0; i < _layers.size(); i++)
    {
        _layers[i].first->update();
        if (_layers[i].second)
            _layers[i].second->update();
    }

    // Clear the dirty flag.
    bool wasDirty = _bIsDirty;
    _bIsDirty     = false;

    return wasDirty;
}

PTScene::PTScene(
    PTRenderer* pRenderer, PTShaderLibrary* pShaderLibrary, uint32_t numRendererDescriptors) :
    SceneBase(pRenderer)
{
    _pRenderer              = pRenderer;
    _pShaderLibrary         = pShaderLibrary;
    _numRendererDescriptors = numRendererDescriptors;

    // Compute the shader record strides.
    _missShaderRecordStride     = HitGroupShaderRecord::stride(); // shader ID, no other parameters
    _missShaderRecordCount      = kBuiltInMissShaderCount; // background, radiance, and shadow
    _hitGroupShaderRecordStride = HitGroupShaderRecord::stride();

    // Use the default environment and ground plane.
    _pGroundPlane = pRenderer->defaultGroundPlane();

    createDefaultResources();
}

ILightPtr PTScene::addLightPointer(const string& lightType)
{
    // Only distant lights are currently supported.
    AU_ASSERT(lightType.compare(Names::LightTypes::kDistantLight) == 0,
        "Only distant lights currently supported");

    // The remaining operations are not yet thread safe.
    std::lock_guard<std::mutex> lock(_mutex);

    // Assign arbritary index to ensure deterministic ordering.
    int index = _currentLightIndex++;

    // Create the light object.
    PTLightPtr pLight = make_shared<PTLight>(this, lightType, index);

    // Add weak pointer to distant light map.
    _distantLights[index] = pLight;

    // Return the new light.
    return pLight;
}

IInstancePtr PTScene::addInstancePointer(const Path& /* path*/, const IGeometryPtr& pGeom,
    const IMaterialPtr& pMaterial, const mat4& transform, const LayerDefinitions& materialLayers)
{
    // Cast the (optional) material to the device implementation. Use the default material if one is
    // not specified.
    PTMaterialPtr pPTMaterial = pMaterial
        ? dynamic_pointer_cast<PTMaterial>(pMaterial)
        : dynamic_pointer_cast<PTMaterial>(_pDefaultMaterialResource->resource());

    // The remaining operations are not yet thread safe.
    std::lock_guard<std::mutex> lock(_mutex);

    // Create the instance object and add it to the list of instances for the scene.
    PTInstancePtr pPTInstance = make_shared<PTInstance>(
        this, dynamic_pointer_cast<PTGeometry>(pGeom), pPTMaterial, transform, materialLayers);

    return pPTInstance;
}

void PTScene::setGroundPlanePointer(const IGroundPlanePtr& pGroundPlane)
{
    // Use the renderer's default ground plane if a ground plane is not specified.
    // NOTE: The default ground plane is disabled ("enabled" == false), so that setting a null
    // pointer on this function will disable the ground plane.
    _pGroundPlane = pGroundPlane ? dynamic_pointer_cast<PTGroundPlane>(pGroundPlane)
                                 : _pRenderer->defaultGroundPlane();
}

ID3D12Resource* PTScene::getMissShaderTable(size_t& recordStride, uint32_t& recordCount) const
{
    // Set the shader record size and count.
    recordStride = _missShaderRecordStride;
    recordCount  = _missShaderRecordCount;

    return _pMissShaderTable.Get();
}

ID3D12Resource* PTScene::getHitGroupShaderTable(size_t& recordStride, uint32_t& recordCount) const
{
    // Set the shader record size and count.
    recordStride = HitGroupShaderRecord::stride();
    recordCount  = static_cast<uint32_t>(_lstInstanceData.size());

    return _pHitGroupShaderTable.Get();
}

// Sort function, used to sort lights to ensure deterministic ordering.
bool compareLights(PTLight* first, PTLight* second)
{
    return (first->index() < second->index());
}

void PTScene::update()
{
    // Update base class.
    SceneBase::update();

    // Update the environment.
    // This is called if *any* active environment objects have changed, as that will almost always
    // be the only active one.
    if (_environments.changedThisFrame())
    {
        _pEnvironment = static_pointer_cast<PTEnvironment>(_pEnvironmentResource->resource());
        _pEnvironment->update();
        _isEnvironmentDescriptorsDirty = true;
    }

    // Update the ground plane.
    _pGroundPlane->update();

    // See if any distant lights have been changed this frame, and build vector of active lights.
    bool distantLightsUpdated = false;
    vector<PTLight*> currLights;
    for (auto iter = _distantLights.begin(); iter != _distantLights.end(); iter++)
    {
        // Get the light and ensure weak pointer still valid.
        PTLightPtr pLight = iter->second.lock();
        if (pLight)
        {
            // Add to currently active light vector.
            currLights.push_back(pLight.get());

            // If the dirty flag is set, GPU data must be updated.
            if (pLight->isDirty())
            {
                distantLightsUpdated = true;
                pLight->clearDirtyFlag();
            }
        }
        else
        {
            // If the weak pointer is not valid remove it from the map, and ensure GPU data is
            // updated (as a light has been removed.)
            _distantLights.erase(iter->first);
            distantLightsUpdated = true;
        }
    }

    // If distant lights have changed update the LightData struct that is passed to the GPU.
    if (distantLightsUpdated)
    {
        // Sort the lights by index, to ensure deterministic ordering.
        sort(currLights.begin(), currLights.end(), compareLights);

        // Set the distant light count to minimum of current light vector size and the max distant
        // light limit.  Lights in the sorted array past LightLimits::kMaxDistantLights are ignored.
        _lights.distantLightCount =
            std::min(int(currLights.size()), int(LightLimits::kMaxDistantLights));

        // Add to the light data buffer that is copied to the frame data for this frame.
        for (int i = 0; i < _lights.distantLightCount; i++)
        {
            // Store the cosine of the radius for use in the shader.
            _lights.distantLights[i].cosRadius =
                cos(0.5f * currLights[i]->asFloat(Names::LightProperties::kAngularDiameter));

            // Invert the direction for use in the shader.
            _lights.distantLights[i].direction =
                -currLights[i]->asFloat3(Names::LightProperties::kDirection);

            // Store color in RGB and intensity in alpha.
            _lights.distantLights[i].colorAndIntensity =
                vec4(currLights[i]->asFloat3(Names::LightProperties::kColor),
                    currLights[i]->asFloat(Names::LightProperties::kIntensity));
        }
    }

    // If any active geometry resources have been modified, flush the vertex buffer pool in case
    // there are any pending vertex buffers that are required to update the geometry, and then
    // update the geometry.
    if (_geometry.changedThisFrame())
    {
        // Iterate through the modified geometry (which will include the newly active ones.)
        for (PTGeometry& geom : _geometry.modified().resources<PTGeometry>())
        {
            geom.update();
        }

        // Flush the vertex buffer pool.
        _pRenderer->flushVertexBufferPool();

    }

    // If any active material resources have been modified update them and build a list of unique
    // samplers for all the active materials.
    if (_materials.changedThisFrame())
    {
        map<size_t, uint32_t> materialSamplerIndicesMap;
        _samplerLookup.clear();
        // Iterate through the all the active materials, even ones that have not changed.
        for (PTMaterial& mtl : _materials.active().resources<PTMaterial>())
        {
            mtl.update();

            // Further ensure combination of samplers in material is unique.
            _samplerLookup.add(mtl);
        }

        // Wait for previous render tasks and then clear the descriptor heap.
        // TODO: Only do this if any texture parameters have changed.
        _pRenderer->waitForTask();
        clearDesciptorHeap();
    }

    // Upload any transfer buffers that have been updated this frame.
    _pRenderer->uploadTransferBuffers();

    // Update the geometry BLAS (after any transfer buffers have been uploaded.)
    if (_geometry.changedThisFrame())
    {
        for (PTGeometry& geom : _geometry.modified().resources<PTGeometry>())
        {
            if (!geom.isIncomplete())
                geom.updateBLAS();
        }

    }

    // If any active instances have been modified or activated, update all the active instances.
    if (_instances.changedThisFrame())
    {
        for (PTInstance& instance : _instances.active().resources<PTInstance>())
        {
            instance.update();
        }
    }

    // Update the acceleration structure if any geometry or instances have been modified.
    if (_instances.changedThisFrame() || _geometry.changedThisFrame())
    {

        // Ensure the acceleration structure is no longer being accessed.
        // TODO: Is there a less drastic stall we can do here?
        _pRenderer->waitForTask();

        // Release the acceleration structure.
        _pAccelStructure.Reset();
    }

    // Update the scene resources: the acceleration structure, the descriptor heap, and the shader
    // tables.  Will only do anything if the relevant pointers have been cleared.
    updateAccelerationStructure();
    updateDescriptorHeap();
    updateShaderTables();
}

void PTScene::clearDesciptorHeap()
{
    _pDescriptorHeap.Reset();
}

void PTScene::clearShaderData()
{
    // Delete the hit group and miss shader table.
    _pHitGroupShaderTable.Reset();
    _pMissShaderTable.Reset();
}

void PTScene::updateAccelerationStructure()
{
    // Do nothing if the acceleration structure already exists.
    if (_pAccelStructure)
    {
        return;
    }

    // Build a list of *unique* instance data objects, i.e. unique combinations of geometry and
    // material (but not transform). This can be used to build a minimal set of shader records, e.g.
    // a large assembly with hundreds of identical screws can have the same shader record for all of
    // the screws. At the same time, assign an identifier (index) so that each instance in the TLAS
    // (built later) can refer to the correct shader record.
    uint32_t nextIndex = 0;
    InstanceDataMap instanceDataMap;
    LayerIndicesMap layerIndicesMap;
    vector<uint32_t> instanceDataIndices;
    map<size_t, uint32_t> materialSamplerIndicesMap;
    instanceDataIndices.reserve(_instances.active().count());
    _lstInstanceData.clear();
    _lstLayerData.clear();

    // Iterate through all the active instances in the scene.
    for (PTInstance& instance : _instances.active().resources<PTInstance>())
    {
        // Find the index of this instance's material within list of active materials.
        uint32_t mtlIndex = _materials.active().findActiveIndex(instance.material());
        AU_ASSERT(mtlIndex != -1, "Material not active");

        // Build the data required to implement material layers.
        vector<int> layerIndices;
        PTLayerIndexTablePtr layerIndexTable;
        if (instance.materialLayers().size())
        {
            auto& materialLayers = instance.materialLayers();
            for (int i = 0; i < materialLayers.size(); i++)
            {
                //  Find the index of this layer's material within list of active materials.
                PTMaterialPtr pLayerMtl = materialLayers[i].first;
                uint32_t layerMtlIndex  = _materials.active().findActiveIndex(pLayerMtl);

                // Build an array of layer indices for each instance.
                layerIndices.push_back(
                    static_cast<int>(_lstLayerData.size()) + kBuiltInMissShaderCount);

                // For each layer material create the layer data required to build shader.
                // TODO: We could remove duplicates here, as we do with the instance data.
                LayerData layerData(materialLayers[i], layerMtlIndex);
                _lstLayerData.push_back(layerData);
            }

            if (layerIndicesMap.find(layerIndices) == layerIndicesMap.end())
            {
                layerIndexTable = make_shared<PTLayerIndexTable>(_pRenderer, layerIndices);
                layerIndicesMap[layerIndices] = layerIndexTable;
            }
            else
                layerIndexTable = layerIndicesMap[layerIndices];
        }

        // If there is no matching instance data in the map, create a new index, add a new instance
        // data object to the map, and add to the (ordered) list of instance data. Otherwise, use
        // the the index for the existing instance data.
        uint32_t instanceDataIndex = 0;
        InstanceData instanceData(instance, layerIndexTable, mtlIndex);
        if (instanceDataMap.find(instanceData) == instanceDataMap.end())
        {
            instanceDataIndex             = nextIndex++;
            instanceDataMap[instanceData] = instanceDataIndex;
            _lstInstanceData.push_back(instanceData);
        }
        else
        {
            instanceDataIndex = instanceDataMap[instanceData];
        }

        for (int i = 0; i < layerIndices.size(); i++)
        {
            int idx                  = layerIndices[i] - kBuiltInMissShaderCount;
            _lstLayerData[idx].index = instanceDataIndex;
        }

        // Add the instance data index to the list, i.e. one per instance in the scene.
        instanceDataIndices.push_back(instanceDataIndex);
    }

    // Build the top-level acceleration structure (TLAS).
    _pAccelStructure = buildTLAS(instanceDataIndices);

    // If the acceleration structure was rebuilt, then the descriptor heap, as well as the miss and
    // hit group shader tables must likewise be rebuilt, as they rely on the instance data.
    _pDescriptorHeap.Reset();
    _pHitGroupShaderTable.Reset();
    _pMissShaderTable.Reset();
}

void PTScene::updateDescriptorHeap()
{
    // Create the descriptor heap if needed.
    if (!_pDescriptorHeap)
    {
        // Get the device.
        ID3D12Device5Ptr pDevice = _pRenderer->dxDevice();

        // Determine the number of texture and sampler descriptors used for all the instances'
        // materials.
        UINT uniqueMaterialDescriptorCount = 0;
        for (PTMaterial& mtl : _materials.active().resources<PTMaterial>())
        {
            uniqueMaterialDescriptorCount += mtl.descriptorCount();
        }

        UINT uniqueMaterialSamplerDescriptorCount = 0;
        for (PTMaterial& mtl : _samplerLookup.unique())
        {
            uniqueMaterialSamplerDescriptorCount += mtl.samplerDescriptorCount();
        }

        // Create a descriptor heap for CBV/SRV/UAVs needed by shader records. Currently this means
        // the descriptors for the instance materials, plus the number of descriptors needed by the
        // renderer and environment.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = uniqueMaterialDescriptorCount + _numRendererDescriptors +
            _pEnvironment->descriptorCount();
        heapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        checkHR(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_pDescriptorHeap)));

        // Create a descriptor heap for samplers needed by shader records. Currently this means
        // the descriptors for the instance (and layer) material samplers.
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors             = uniqueMaterialSamplerDescriptorCount;
        samplerHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        checkHR(pDevice->CreateDescriptorHeap(
            &samplerHeapDesc, IID_PPV_ARGS(&_pSamplerDescriptorHeap)));

        // If the descriptor heap was rebuilt, then the descriptors must likewise be recreated.
        _isEnvironmentDescriptorsDirty = true;
        _isHitGroupDescriptorsDirty    = true;
    }

    // Get a CPU handle to the start of the sampler descriptor heap, offset by the number of
    // descriptors reserved for the renderer.
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT handleIncrement = _pRenderer->dxDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    handle.Offset(_numRendererDescriptors, handleIncrement);

    // Update the environment descriptors if needed.
    if (_isEnvironmentDescriptorsDirty)
    {
        // Create the descriptors for the environment textures.
        // NOTE: This will also increment the handle past the new descriptors.
        _pEnvironment->createDescriptors(handle, handleIncrement);

        // Clear the dirty flag.
        _isEnvironmentDescriptorsDirty = false;
    }

    // Update the hit group descriptors if needed.
    if (_isHitGroupDescriptorsDirty)
    {
        // Iterate the list of active materials, creating descriptors for each as needed from the
        // material.
        for (PTMaterial& mtl : _materials.active().resources<PTMaterial>())
        {
            // Create the descriptors for the material's textures.
            // NOTE: This will also increment the handle past the new descriptors.
            mtl.createDescriptors(handle, handleIncrement);
        }
    }

    // Get a CPU handle to the start of the sampler descriptor heap.
    CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(
        _pSamplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT samplerHandleIncrement = _pRenderer->dxDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    if (_isHitGroupDescriptorsDirty)
    {
        // Iterate the list of unique samplers, creating sampler descriptors for each as needed.
        for (PTMaterial& mtl : _samplerLookup.unique())
        {
            // Create the descriptors for the material's samplers.
            // NOTE: This will also increment the handle past the new descriptors.
            mtl.createSamplerDescriptors(samplerHandle, samplerHandleIncrement);
        }

        // Clear the dirty flag.
        _isHitGroupDescriptorsDirty = false;
    }
}

void PTScene::updateShaderTables()
{

    // Create and populate the hit  shader table if it doesn't exist and there are instances.
    auto instanceCount = static_cast<uint32_t>(_instances.active().count());

    // Texture and sampler descriptors for unique materials in the scene.
    vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> lstUniqueMaterialTextureDescriptors;
    vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> lstUniqueMaterialSamplerDescriptors;

    // Get currently active materials.
    auto& activeMaterialResources = _materials.active().resources<PTMaterial>();

    if (!_pHitGroupShaderTable && instanceCount > 0)
    {
        // Build a list of texture and sampler GPU handles for all unique materials.
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
                _pDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            UINT handleIncrement = _pRenderer->dxDevice()->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // Offset texture handle by the number of descriptors reserved for the renderer and
            // environment.
            handle.Offset(
                _numRendererDescriptors + _pEnvironment->descriptorCount(), handleIncrement);

            for (size_t i = 0; i < _materials.active().count(); i++)
            {
                // Store the handle.
                lstUniqueMaterialTextureDescriptors.push_back(handle);

                // Increment the handle by the number of descriptors for each material.
                handle.Offset(PTMaterial::descriptorCount(), handleIncrement);
            }

            CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle(
                _pSamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            UINT samplerHandleIncrement = _pRenderer->dxDevice()->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

            for (int i = 0; i < _samplerLookup.unique().size(); i++)
            {
                // Store the handle.
                lstUniqueMaterialSamplerDescriptors.push_back(samplerHandle);

                // Increment the sampler handle by the number of sample descriptors for each
                // material.
                samplerHandle.Offset(PTMaterial::samplerDescriptorCount(), samplerHandleIncrement);
            }
        }

        size_t recordStride = HitGroupShaderRecord::stride();

        // Create a transfer buffer for the shader table, and map it for writing.
        size_t shaderTableSize          = recordStride * instanceCount;
        TransferBuffer hitGroupTransferBuffer = _pRenderer->createTransferBuffer(shaderTableSize, "HitGroupShaderTable");
        uint8_t* pShaderTableMappedData = hitGroupTransferBuffer.map();

        // Retain the GPU buffer from the transfer buffer, the upload buffer will be deleted by the renderer once upload complete.
        _pHitGroupShaderTable = hitGroupTransferBuffer.pGPUBuffer;

        // Iterate the instance data objects, creating a hit group shader record for each one, and
        // copying the shader record data to the shader table.
        for (int i = 0; i < _lstInstanceData.size(); i++)
        {
            const auto& instanceData = _lstInstanceData[i];
            // Get the hit group shader ID from the material shader, which will change if the shader
            // library is rebuilt.
            PTMaterial& instanceMtl = activeMaterialResources[instanceData.mtlIndex];
            const DirectXShaderIdentifier hitGroupShaderID =
                _pShaderLibrary->getShaderID(instanceMtl.shader());

            // Lookup texture and sampler handle for instance.
            CD3DX12_GPU_DESCRIPTOR_HANDLE mtlTextureHandle =
                lstUniqueMaterialTextureDescriptors[instanceData.mtlIndex];
            CD3DX12_GPU_DESCRIPTOR_HANDLE mtlSamplerHandle =
                lstUniqueMaterialSamplerDescriptors[_samplerLookup.getUniqueIndex(
                    instanceData.mtlIndex)];

            // Shader record data includes the geometry buffers, the material constant buffer, and
            // the descriptor table (offset into the SRV heap) needed for textures.
            PTGeometry::GeometryBuffers geometryBuffers = instanceData.pGeometry->buffers();
            ID3D12Resource* pMaterialLayerIndexBuffer =
                instanceData.pLayerIndices ? instanceData.pLayerIndices->buffer() : nullptr;
            int materialLayerCount =
                instanceData.pLayerIndices ? instanceData.pLayerIndices->count() : 0;
            HitGroupShaderRecord record(hitGroupShaderID, geometryBuffers, instanceMtl,
                pMaterialLayerIndexBuffer, mtlTextureHandle, mtlSamplerHandle, materialLayerCount);
            record.copyTo(pShaderTableMappedData);
            pShaderTableMappedData += recordStride;
        }

        // Close the shader table buffer.
        hitGroupTransferBuffer.unmap();
    }
    // Create and populate the miss shader table if necessary.
    if (!_pMissShaderTable)
    {
        // Calculate miss shader record count (built-ins plus all the layer material miss shaders)
        _missShaderRecordCount = kBuiltInMissShaderCount + (uint32_t)_lstLayerData.size();

        // Create a buffer for the shader table and write the shader identifiers for the miss
        // shaders.
        // NOTE: There are no arguments (and indeed no local root signatures) for these shaders.
        size_t shaderTableSize          = _missShaderRecordStride * _missShaderRecordCount;

        TransferBuffer missShaderTableTransferBuffer =
            _pRenderer->createTransferBuffer(shaderTableSize, "MissShaderTable");
        _pMissShaderTable = missShaderTableTransferBuffer.pGPUBuffer;

        uint8_t* pShaderTableMappedData = missShaderTableTransferBuffer.map();
        uint8_t* pEndOfShaderTableMappedData = pShaderTableMappedData + shaderTableSize;

        // Get the shader identifiers from the shader library, which will change if the library is
        // rebuilt.
        // NOTE: The first miss shader is a null shader, used when a miss shader is not needed.
        static array<uint8_t, SHADER_ID_SIZE> kNullShaderID = { 0 };
        ::memcpy_s(pShaderTableMappedData, SHADER_ID_SIZE, kNullShaderID.data(), SHADER_ID_SIZE);
        pShaderTableMappedData += _missShaderRecordStride;
        ::memcpy_s(pShaderTableMappedData, SHADER_ID_SIZE,
            _pShaderLibrary->getSharedEntryPointShaderID(EntryPointTypes::kBackgroundMiss),
            SHADER_ID_SIZE);
        pShaderTableMappedData += _missShaderRecordStride;
        ::memcpy_s(pShaderTableMappedData, SHADER_ID_SIZE,
            _pShaderLibrary->getSharedEntryPointShaderID(EntryPointTypes::kRadianceMiss),
            SHADER_ID_SIZE);
        pShaderTableMappedData += _missShaderRecordStride;
        ::memcpy_s(pShaderTableMappedData, SHADER_ID_SIZE,
            _pShaderLibrary->getSharedEntryPointShaderID(EntryPointTypes::kShadowMiss),
            SHADER_ID_SIZE);
        pShaderTableMappedData += _missShaderRecordStride;

        // Fill in layer material miss shaders..
        for (int i = 0; i < _lstLayerData.size(); i++)
        {
            // Get the layer data and the parent instance data for this layer.
            auto& layerData          = _lstLayerData[i];
            auto& parentInstanceData = _lstInstanceData[layerData.index];
            PTMaterial& layerMtl     = activeMaterialResources[layerData.instanceData.mtlIndex];

            // Get the material shader
            auto pShader = layerMtl.shader();

            // Create the geometry by merging the layer geometry with parent instance's geometry.
            PTGeometry::GeometryBuffers geometryLayerBuffers =
                parentInstanceData.pGeometry->buffers();
            if (layerData.instanceData.pGeometry)
            {
                // Ensure the layer geometry matches parent geometry
                if (layerData.instanceData.pGeometry->vertexCount() !=
                    parentInstanceData.pGeometry->vertexCount())
                {
                    // If vertex counts don't match raise an error and then fail.
                    AU_ERROR(
                        "Layer geometry %s vertex count (%d) does not match base geometry %s "
                        "vertex count "
                        "(%d) for instance at index %d",
                        layerData.instanceData.pGeometry->name().c_str(),
                        layerData.instanceData.pGeometry->vertexCount(),
                        parentInstanceData.pGeometry->name().c_str(),
                        parentInstanceData.pGeometry->vertexCount(), layerData.index);

                    // This will be caught by the try statement around render().
                    AU_FAIL("Invalid geometry");
                }

                // Set any UVs from the layer geometry.
                if (layerData.instanceData.pGeometry->buffers().TexCoordBuffer)
                {
                    geometryLayerBuffers.TexCoordBuffer =
                        layerData.instanceData.pGeometry->buffers().TexCoordBuffer;
                }

                // Set any normals from the layer geometry.
                if (layerData.instanceData.pGeometry->buffers().NormalBuffer)
                    geometryLayerBuffers.NormalBuffer =
                        layerData.instanceData.pGeometry->buffers().NormalBuffer;

                // Set any positions from the layer geometry.
                if (layerData.instanceData.pGeometry->buffers().PositionBuffer)
                    geometryLayerBuffers.PositionBuffer =
                        layerData.instanceData.pGeometry->buffers().PositionBuffer;
            }

            // Lookup texture and sampler handle for layer.
            CD3DX12_GPU_DESCRIPTOR_HANDLE mtlTextureHandle =
                lstUniqueMaterialTextureDescriptors[layerData.instanceData.mtlIndex];
            CD3DX12_GPU_DESCRIPTOR_HANDLE mtlSamplerHandle =
                lstUniqueMaterialSamplerDescriptors[_samplerLookup.getUniqueIndex(
                    layerData.instanceData.mtlIndex)];

            // Create hit group (as layer miss shader has same layout as luminance closest hit
            // shader)
            HitGroupShaderRecord layerRecord(_pShaderLibrary->getLayerShaderID(pShader),
                geometryLayerBuffers, layerMtl, nullptr, mtlTextureHandle, mtlSamplerHandle, 0);
            layerRecord.copyTo(pShaderTableMappedData);
            pShaderTableMappedData += _missShaderRecordStride;
        }

        // Ensure we didn't over run the shader table buffer.
        AU_ASSERT(pShaderTableMappedData == pEndOfShaderTableMappedData, "Shader table overrun");

        // Unmap the table.
        missShaderTableTransferBuffer.unmap(); // no HRESULT
    }
}

ID3D12ResourcePtr PTScene::buildTLAS(const vector<uint32_t>& instanceDataIndices)
{
    // Create and populate a buffer with instance data, if there are any instances.
    auto instanceCount = static_cast<uint32_t>(_instances.active().count());
    ID3D12ResourcePtr pInstanceBuffer;
    if (instanceCount > 0)
    {
        // Create a buffer for the instance data.
        size_t bufferSize            = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount;
        pInstanceBuffer              = _pRenderer->createBuffer(bufferSize);
        uint8_t* pInstanceMappedData = nullptr;
        checkHR(pInstanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pInstanceMappedData)));

        // Describe a set of instances with varying geometries and transforms.
        uint32_t instanceIndex = 0;
        for (PTInstance& instance : _instances.active().resources<PTInstance>())
        {
            // Get the bottom-level acceleration structure (BLAS) from the instance geometry.
            ID3D12ResourcePtr pBLAS = instance.dxGeometry()->blas();

            // Get the transpose of the transform matrix of the instance. GLM has column-major
            // matrices, but DXR expects (4x3) row-major matrices for instance descriptions.
            mat4 matrix = transpose(instance.transform());

            // Describe the instance. Specifically this includes:
            // - A pointer to the BLAS.
            // - The transform for the instance.
            // - An identifier for the hit group data to use when the instance is hit by a ray.
            // NOTE: The instance is not set as opaque here on the instance flags, so that the any
            // hit shader can be called if needed. If the any hit shader is not needed, the shader
            // will use the opaque ray flag when calling TraceRay().
            // NOTE: Using memcpy() for the transform copy writes too much data (all 16 floats) in
            // *release builds*, for an unknown reason. sizeof(instanceDesc.Transform) is only 12
            // floats. Using memcpy_s() does not cause this problem, and it should be used
            // everywhere to be safe!
            D3D12_RAYTRACING_INSTANCE_DESC instanceDesc      = {};
            instanceDesc.AccelerationStructure               = pBLAS->GetGPUVirtualAddress();
            instanceDesc.InstanceID                          = 0;
            instanceDesc.InstanceMask                        = 0xFF;
            instanceDesc.InstanceContributionToHitGroupIndex = instanceDataIndices[instanceIndex++];
            ::memcpy_s(instanceDesc.Transform, sizeof(instanceDesc.Transform), &matrix,
                sizeof(instanceDesc.Transform));
            instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

            // Copy the instance description to the buffer.
            ::memcpy_s(pInstanceMappedData, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), &instanceDesc,
                sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
            pInstanceMappedData += sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
        }

        // Close the instance buffer.
        pInstanceBuffer->Unmap(0, nullptr); // no HRESULT
    }

    // Describe the top-level acceleration structure (TLAS).
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.DescsLayout   = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.Flags         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    tlasInputs.NumDescs      = instanceCount;
    tlasInputs.InstanceDescs = pInstanceBuffer ? pInstanceBuffer->GetGPUVirtualAddress() : 0;

    // Get the sizes required for the TLAS scratch and result buffers, and create them.
    // NOTE: The scratch buffer is obtained from the renderer and will be retained for the duration
    // of the build task started below, and then it will be released.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo = {};
    _pRenderer->dxDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);
    D3D12_GPU_VIRTUAL_ADDRESS tlasScratchAddress =
        _pRenderer->getScratchBuffer(tlasInfo.ScratchDataSizeInBytes);
    ID3D12ResourcePtr pTLAS = _pRenderer->createBuffer(tlasInfo.ResultDataMaxSizeInBytes,
        "TLAS Buffer", D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    // Describe the build for the TLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
    tlasDesc.Inputs                                             = tlasInputs;
    tlasDesc.ScratchAccelerationStructureData                   = tlasScratchAddress;
    tlasDesc.DestAccelerationStructureData                      = pTLAS->GetGPUVirtualAddress();

    // Build the TLAS using a command list. Insert a UAV barrier so that it can't be used until
    // it is generated.
    ID3D12GraphicsCommandList4Ptr pCommandList = _pRenderer->beginCommandList();
    pCommandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);
    _pRenderer->addUAVBarrier(pTLAS.Get());

    // Complete the command list and task, and wait for the work to complete.
    // NOTE: This waits so that the instance buffer doesn't have to be retained beyond this
    // function. Since there is only one TLAS for a scene, this is not a practical bottleneck.
    _pRenderer->submitCommandList();
    _pRenderer->completeTask();
    _pRenderer->waitForTask();

    return pTLAS;
}

END_AURORA
