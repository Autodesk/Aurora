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

#include "HGIEnvironment.h"
#include "HGIGeometry.h"
#include "HGIImage.h"
#include "HGIMaterial.h"
#include "SceneBase.h"

BEGIN_AURORA

// Forward declarations.
class HGIRenderer;
struct Consolidator;
class Transpiler;

// An internal implementation for IInstance.
class HGIInstance : public IInstance
{
public:
    // Constructor.
    HGIInstance(HGIRenderer* pRenderer, shared_ptr<HGIMaterial> pMaterial,
        shared_ptr<HGIGeometry> pGeom, const mat4& transform);

    void setMaterial(const IMaterialPtr& pMaterial) override
    {
        _pMaterial = dynamic_pointer_cast<HGIMaterial>(pMaterial);
    }
    void setTransform(const mat4& transform) override
    {
        const pxr::GfMatrix4f* pGFMtx = reinterpret_cast<const pxr::GfMatrix4f*>(&transform);
        _transform                    = pGFMtx->GetTranspose();
    }
    void setObjectIdentifier(int /*objectId*/) override {}

    void setVisible(bool /*val*/) override {}

    IGeometryPtr geometry() const override { return dynamic_pointer_cast<IGeometry>(_pGeometry); }

    const pxr::GfMatrix4f transform() { return _transform; }
    shared_ptr<HGIGeometry> hgiGeometry() { return _pGeometry; }
    shared_ptr<HGIMaterial> material() { return _pMaterial; }

private:
    shared_ptr<HGIGeometry> _pGeometry;
    shared_ptr<HGIMaterial> _pMaterial;
    [[maybe_unused]] HGIRenderer* _pRenderer;
    pxr::GfMatrix4f _transform;
};

using HGIInstancePtr = std::shared_ptr<HGIInstance>;

// Shader record for hit shaders.
struct InstanceShaderRecord
{
    // Geometry data.
    HGIGeometryBuffers geometry;

    // Index into material array.
    uint64_t material;

    // Index into texture sampler array for material's textures.
    int baseColorTextureIndex = -1;

    int specularRoughnessTextureIndex = -1;
    int normalTextureIndex            = -1;
    int opacityTextureIndex           = -1;

    // Geometry flags.
    unsigned int hasNormals   = true;
    unsigned int hasTexCoords = true;
};

// Per-instance data.
struct InstanceData
{
    HGIInstance& instance;
    InstanceShaderRecord shaderRecord;
};

// An internal implementation for IScene.
class HGIScene : public SceneBase
{
public:
    // Constructor and destructor.
    HGIScene(HGIRenderer* pRenderer);

    /*** IScene Functions ***/
    void setGroundPlanePointer(const IGroundPlanePtr& pGroundPlane) override;

    pxr::HgiRayTracingPipelineHandle rayTracingPipeline() { return _rayTracingPipeline->handle(); }

    bool update();
    void rebuildInstanceList();
    void createResources();
    void rebuildPipeline();
    void rebuildAccelerationStructure();
    void rebuildResourceBindings();

    pxr::HgiAccelerationStructureHandle tlas() { return _tlas->handle(); }

    pxr::HgiResourceBindingsHandle resourceBindings() { return _resBindings->handle(); }

    IInstancePtr addInstancePointer(const Path& /* path*/, const IGeometryPtr& pGeom,
        const IMaterialPtr& pMaterial, const mat4& transform,
        const LayerDefinitions& materialLayers) override;

private:
    HGIRenderer* _pRenderer = nullptr;
    shared_ptr<Transpiler> _transpiler;
    HgiRayTracingPipelineHandleWrapper::Pointer _rayTracingPipeline;
    HgiAccelerationStructureHandleWrapper::Pointer _tlas;
    HgiAccelerationStructureGeometryHandleWrapper::Pointer _tlasGeom;
    vector<InstanceData> _lstInstances;
    vector<shared_ptr<HGIImage>> _lstImages;
    HgiResourceBindingsHandleWrapper::Pointer _resBindings;
    HgiTextureHandleWrapper::Pointer _pDefaultImage;
    HgiShaderFunctionHandleWrapper::Pointer _backgroundMissShaderFunc;
    HgiShaderFunctionHandleWrapper::Pointer _rayGenShaderFunc;
    HgiShaderFunctionHandleWrapper::Pointer _shadowMissShaderFunc;
    HgiShaderFunctionHandleWrapper::Pointer _radianceMissShaderFunc;
    HgiShaderFunctionHandleWrapper::Pointer _closestHitShaderFunc;
};
using HGIScenePtr = std::shared_ptr<HGIScene>;

END_AURORA
