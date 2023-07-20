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

#include "PTEnvironment.h"
#include "PTGeometry.h"
#include "PTGroundPlane.h"
#include "PTLight.h"
#include "PTMaterial.h"
#include "SceneBase.h"

#include <mutex>

BEGIN_AURORA

// Forward declarations.
class PTRenderer;
class PTScene;
class PTShaderLibrary;

// Definition of a layer material (material+geometry)
using PTLayerDefinition = pair<PTMaterialPtr, PTGeometryPtr>;

// An internal implementation for the table of layer material indices.
class PTLayerIndexTable
{
public:
    /*** Lifetime Management ***/

    PTLayerIndexTable(PTRenderer* pRenderer, const vector<int>& indices = {});
    void set(const vector<int>& indices);

    // The maximum number of supported material layers, must match value in PathTracingCommon.hlsl
    // shader.
    static const int kMaxMaterialLayers = 64;

    int count() { return _count; }
    ID3D12Resource* buffer() const { return _pConstantBuffer.Get(); }

private:
    int _count             = 0;
    PTRenderer* _pRenderer = nullptr;
    ID3D12ResourcePtr _pConstantBuffer;
};
MAKE_AURORA_PTR(PTLayerIndexTable);

// An internal implementation for IInstance.
class PTInstance : public IInstance
{
public:
    /*** Lifetime Management ***/

    PTInstance(PTScene* pScene, const PTGeometryPtr& pGeometry, const PTMaterialPtr& pMaterial,
        const mat4& transform, const LayerDefinitions& layers);
    virtual ~PTInstance();

    /*** IInstance Functions ***/

    void setMaterial(const IMaterialPtr& pMaterial) override;
    void setTransform(const mat4& transform) override;
    void setObjectIdentifier(int objectId) override;
    IGeometryPtr geometry() const override { return dynamic_pointer_cast<IGeometry>(_pGeometry); }
    void setVisible(bool /*val*/) override {}

    /*** Functions ***/

    PTGeometryPtr dxGeometry() const { return _pGeometry; }
    PTMaterialPtr material() const { return _pMaterial; }
    const mat4& transform() const { return _transform; }
    bool update();

    const vector<PTLayerDefinition>& materialLayers() const { return _layers; };

private:
    /*** Private Variables ***/

    PTScene* _pScene = nullptr;
    bool _bIsDirty   = true;
    PTGeometryPtr _pGeometry;
    PTMaterialPtr _pMaterial;
    mat4 _transform;

    vector<PTLayerDefinition> _layers;
};
MAKE_AURORA_PTR(PTInstance);

// An internal implementation for IScene.
class PTScene : public SceneBase
{
public:
    /*** Lifetime Management ***/
    PTScene(
        PTRenderer* pRenderer, PTShaderLibrary* pShaderLibrary, uint32_t numRendererDescriptors);
    ~PTScene() = default;

    /*** IScene Functions ***/
    void setGroundPlanePointer(const IGroundPlanePtr& pGroundPlane) override;
    IInstancePtr addInstancePointer(const Path& path, const IGeometryPtr& geom,
        const IMaterialPtr& pMaterial, const mat4& transform,
        const LayerDefinitions& materialLayers) override;
    ILightPtr addLightPointer(const string& lightType) override;

    void update();

    /*** Functions ***/

    int instanceCount() const { return static_cast<int>(_instances.active().count()); }
    PTEnvironmentPtr environment() const { return _pEnvironment; }
    PTGroundPlanePtr groundPlane() const { return _pGroundPlane; }
    ID3D12DescriptorHeap* descriptorHeap() const { return _pDescriptorHeap.Get(); }
    ID3D12DescriptorHeap* samplerDescriptorHeap() const { return _pSamplerDescriptorHeap.Get(); }
    ID3D12Resource* accelerationStructure() const { return _pAccelStructure.Get(); }
    ID3D12Resource* getMissShaderTable(size_t& recordStride, uint32_t& recordCount) const;
    ID3D12Resource* getHitGroupShaderTable(size_t& recordStride, uint32_t& recordCount) const;
    void clearShaderData();
    void clearDesciptorHeap();
    PTRenderer* renderer() { return _pRenderer; }

private:
    /*** Private Types ***/

    // A structure containing the contents of instance that uniquely identify it, for purposes
    // of using it in a shader table.
    struct InstanceData
    {
        InstanceData() : pGeometry(nullptr), mtlIndex((uint32_t)-1), pLayerIndices(nullptr) {}

        InstanceData(
            const PTInstance& instance, PTLayerIndexTablePtr pLayerIndices, uint32_t mtlIndex) :
            pGeometry(instance.dxGeometry()), mtlIndex(mtlIndex), pLayerIndices(pLayerIndices)
        {
        }

        bool operator==(const InstanceData& other) const
        {
            return pGeometry == other.pGeometry && mtlIndex == other.mtlIndex &&
                pLayerIndices == other.pLayerIndices;
        }

        PTGeometryPtr pGeometry;
        PTLayerIndexTablePtr pLayerIndices;
        // Index into array of unique materials for scsne.
        uint32_t mtlIndex;
    };

    // Structure containing the contents of material layer, and an index back to parent instance
    // data.
    struct LayerData
    {
        LayerData(const PTLayerDefinition& def, uint32_t mtlIndex, int idx = -1) : index(idx)
        {
            instanceData.mtlIndex  = mtlIndex;
            instanceData.pGeometry = def.second;
        }

        InstanceData instanceData;
        int index;
    };

    // A functor that hashes the contents of an instance, i.e. the pointers to the geometry and
    // material.
    struct HashInstanceData
    {
        size_t operator()(const InstanceData& object) const
        {
            hash<const PTGeometry*> hasher1;
            hash<uint32_t> hasher2;
            hash<const PTLayerIndexTable*> hasher3;
            return hasher1(object.pGeometry.get()) ^ (hasher2(object.mtlIndex) << 1) ^
                (hasher3(object.pLayerIndices.get()) << 2);
        }
    };

    using InstanceList     = set<PTInstancePtr>;
    using InstanceDataMap  = unordered_map<InstanceData, uint32_t, HashInstanceData>;
    using LayerIndicesMap  = map<vector<int>, PTLayerIndexTablePtr>;
    using InstanceDataList = vector<InstanceData>; // <set> not needed; known to be unique
    using LayerDataList    = vector<LayerData>;

    /*** Private Functions ***/

    void updateAccelerationStructure();
    void updateDescriptorHeap();
    void updateShaderTables();
    ID3D12ResourcePtr buildTLAS(const vector<uint32_t>& instanceDataIndices);
    static size_t GetSamplerHash(const PTMaterial& mtl) { return mtl.computeSamplerHash(); }

    /*** Private Variables ***/

    PTRenderer* _pRenderer           = nullptr;
    PTShaderLibrary* _pShaderLibrary = nullptr;
    InstanceDataList _lstInstanceData;
    UniqueHashLookup<PTMaterial, GetSamplerHash> _samplerLookup;
    LayerDataList _lstLayerData;
    PTGroundPlanePtr _pGroundPlane;
    PTEnvironmentPtr _pEnvironment;
    uint32_t _numRendererDescriptors = 0;
    map<int, weak_ptr<PTLight>> _distantLights;
    int _currentLightIndex = 0;

    /*** DirectX 12 Objects ***/

    ID3D12ResourcePtr _pAccelStructure;
    ID3D12DescriptorHeapPtr _pDescriptorHeap;
    ID3D12DescriptorHeapPtr _pSamplerDescriptorHeap;
    ID3D12ResourcePtr _pMissShaderTable;
    size_t _missShaderRecordStride  = 0;
    uint32_t _missShaderRecordCount = 0;
    ID3D12ResourcePtr _pHitGroupShaderTable;
    size_t _hitGroupShaderRecordStride  = 0;
    bool _isEnvironmentDescriptorsDirty = true;
    bool _isHitGroupDescriptorsDirty    = true;
    std::mutex _mutex;
};
MAKE_AURORA_PTR(PTScene);

END_AURORA
