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
    int computeMaterialTextureCount();
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
    PTShaderLibrary& shaderLibrary() { return *_pShaderLibrary.get(); }
    IMaterialPtr createMaterialPointer(
        const string& materialType, const string& document, const string& name);
    shared_ptr<MaterialShader> generateMaterialX(
        const string& document, shared_ptr<MaterialDefinition>* pDefOut);
    void setUnit(const string& unit);

private:
    /*** Private Types ***/

    /*** Private Functions ***/

    void updateAccelerationStructure();
    void updateDescriptorHeap();
    void updateShaderTables();
    ID3D12ResourcePtr buildTLAS();

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;
    unique_ptr<PTShaderLibrary> _pShaderLibrary;
    map<PTMaterial*, int> _materialOffsetLookup;
    map<PTInstance*, int> _instanceOffsetLookup;
    size_t _instanceBufferSize = 0;
    map<PTImage*, int> _materialTextureIndexLookup;
    vector<PTImage*> _activeMaterialTextures;
    PTGroundPlanePtr _pGroundPlane;
    PTEnvironmentPtr _pEnvironment;
    uint32_t _numRendererDescriptors = 0;
    map<int, weak_ptr<PTLight>> _distantLights;
    int _currentLightIndex = 0;
    TransferBuffer _globalMaterialBuffer;
    TransferBuffer _globalInstanceBuffer;

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

    // Code generator used to generate MaterialX files.
#if ENABLE_MATERIALX
    unique_ptr<MaterialXCodeGen::MaterialGenerator> _pMaterialXGenerator;
#endif
};

MAKE_AURORA_PTR(PTScene);

END_AURORA
