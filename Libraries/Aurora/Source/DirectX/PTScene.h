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
#include "PTSampler.h"
#include "SceneBase.h"

#include <mutex>

BEGIN_AURORA

namespace MaterialXCodeGen
{
class MaterialGenerator;
} // namespace MaterialXCodeGen

// -1 is used to indicate an invalid offset in offset buffers passed to GPU.
#define kInvalidOffset -1

// Forward declarations.
class PTRenderer;
class PTScene;
class PTShaderLibrary;
class PTSampler;

// Definition of a layer material (material+geometry)
using PTLayerDefinition = pair<PTMaterialPtr, PTGeometryPtr>;

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

class PTShaderOptions;

// An internal implementation for IScene.
class PTScene : public SceneBase
{
public:
    /*** Lifetime Management ***/
    PTScene(PTRenderer* pRenderer, uint32_t numRendererDescriptors);
    ~PTScene() = default;

    /*** IScene Functions ***/
    void setGroundPlanePointer(const IGroundPlanePtr& pGroundPlane) override;
    IInstancePtr addInstancePointer(const Path& path, const IGeometryPtr& geom,
        const IMaterialPtr& pMaterial, const mat4& transform,
        const LayerDefinitions& materialLayers) override;
    ILightPtr addLightPointer(const string& lightType) override;

    void update();
    void updateResources();

    /*** Functions ***/
    void computeMaterialTextureCount(int& textureCountOut, int& samplerCountOut);
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

    const TransferBuffer& globalMaterialBuffer() { return _globalMaterialBuffer; }
    const TransferBuffer& globalInstanceBuffer() { return _globalInstanceBuffer; }
    const TransferBuffer& layerGeometryBuffer() { return _layerGeometryBuffer; }
    const TransferBuffer& transformMatrixBuffer() { return _transformMatrixBuffer; }
    PTShaderLibrary& shaderLibrary() { return *_pShaderLibrary.get(); }
    IMaterialPtr createMaterialPointer(
        const string& materialType, const string& document, const string& name);
    shared_ptr<MaterialShader> generateMaterialX(
        const string& document, shared_ptr<MaterialDefinition>* pDefOut);
    void setUnit(const string& unit);

private:
    /*** Private Types ***/

    // A structure containing the contents of instance that uniquely identify it, for purposes
    // of using it in a shader table.
    struct InstanceData
    {
        InstanceData(const PTInstance& instance) :
            pGeometry(nullptr),
            mtlBufferOffset((int)-1),
            layers({}),
            bufferOffset(-1),
            isOpaque(true),
            pInstance(&instance)
        {
        }

        bool operator==(const InstanceData& other) const
        {
            if (pGeometry != other.pGeometry || mtlBufferOffset != other.mtlBufferOffset ||
                layers.size() != other.layers.size())
                return false;

            for (int i = 0; i < layers.size(); i++)
            {
                if (layers[i].first != other.layers[i].first)
                    return false;
                if (layers[i].second != other.layers[i].second)
                    return false;
            }
            return true;
        }

        // Properties used to compute hash.
        // Geometry for instance
        PTGeometryPtr pGeometry;
        // Offset in global material buffer and layer geometry buffer for each layer.
        vector<pair<int, int>> layers;
        // Offset into global material buffer for base layer material.
        int mtlBufferOffset;

        // Convenience properties, do not effect hash.
        const PTInstance* pInstance;
        bool isOpaque;
        int bufferOffset;
    };

    // A functor that hashes the contents of an instance, i.e. the pointers to the geometry and
    // material.
    struct HashInstanceData
    {
        size_t operator()(const InstanceData& object) const
        {
            hash<const PTGeometry*> geomHasher;
            hash<int> indexHasher;
            size_t res =
                geomHasher(object.pGeometry.get()) ^ (indexHasher(object.mtlBufferOffset) << 1);
            for (int i = 0; i < object.layers.size(); i++)
            {
                size_t layerHash = indexHasher(object.layers[i].first) ^
                    (indexHasher(object.layers[i].second) << 1);
                res = res ^ (layerHash << (i + 1));
            }
            return res;
        }
    };

    using InstanceList     = set<PTInstancePtr>;
    using InstanceDataMap  = unordered_map<InstanceData, int, HashInstanceData>;
    using InstanceDataList = vector<InstanceData>; // <set> not needed; known to be unique

    /*** Private Functions ***/

    void updateAccelerationStructure();
    void updateDescriptorHeap();
    void updateShaderTables();
    ID3D12ResourcePtr buildTLAS();
    InstanceData createInstanceData(const PTInstance& instance);

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    unique_ptr<PTShaderLibrary> _pShaderLibrary;
    InstanceDataList _lstInstanceData;
    map<const PTMaterial*, int> _materialOffsetLookup;
    map<const PTInstance*, int> _instanceDataIndexLookup;
    map<const PTGeometry*, int> _layerGeometryOffsetLookup;
    vector<PTGeometry*> _layerGeometry;
    size_t _instanceBufferSize        = 0;
    size_t _layerGeometryBufferSize   = 0;
    size_t _transformMatrixBufferSize = 0;
    map<IImage*, int> _materialTextureIndexLookup;
    map<ISampler*, int> _materialSamplerIndexLookup;
    vector<PTImage*> _activeMaterialTextures;
    vector<PTSampler*> _activeMaterialSamplers;
    PTGroundPlanePtr _pGroundPlane;
    PTEnvironmentPtr _pEnvironment;
    uint32_t _numRendererDescriptors = 0;
    map<int, weak_ptr<PTLight>> _distantLights;
    int _currentLightIndex = 0;
    TransferBuffer _globalMaterialBuffer;
    TransferBuffer _globalInstanceBuffer;
    TransferBuffer _layerGeometryBuffer;
    TransferBuffer _transformMatrixBuffer;

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

    map<size_t, ISamplerPtr> _samplerCache;
    map<string, weak_ptr<IImage>> _imageCache;
    // Code generator used to generate MaterialX files.
#if ENABLE_MATERIALX
    unique_ptr<MaterialXCodeGen::MaterialGenerator> _pMaterialXGenerator;
#endif
};

MAKE_AURORA_PTR(PTScene);

END_AURORA
