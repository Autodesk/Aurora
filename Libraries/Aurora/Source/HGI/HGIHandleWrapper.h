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

BEGIN_AURORA

// Wrapper class that handles memory management of HGI handles.
// DestroyFunction is invoked when an instance of this class is destroyed.
template <typename HandleClass, auto DestroyFunction>
class HgiHandleWrapper
{
public:
    using Pointer = unique_ptr<HgiHandleWrapper<HandleClass, DestroyFunction>>;

    HgiHandleWrapper(HandleClass hdl, pxr::Hgi* pHgi) : _hdl(hdl), _pHgi(pHgi) {}
    ~HgiHandleWrapper() { clear(); }

    // Create a unique point to this handle.  DestroyFunction will be invoked when pointer is
    // released.
    static Pointer create(HandleClass hdl, pxr::HgiUniquePtr& pHgi)
    {
        return make_unique<HgiHandleWrapper<HandleClass, DestroyFunction>>(hdl, pHgi.get());
    }

    // Get HGI handle that is being wrapped.
    const HandleClass& handle() const { return _hdl; }

private:
    void clear()
    {
        if (_pHgi)
            DestroyFunction(&_hdl, _pHgi);

        _pHgi = nullptr;
        _hdl  = HandleClass();
    }

    HandleClass _hdl;
    pxr::Hgi* _pHgi;
};

class HGIRenderBuffer;

// Destroy functions for all the HGI handle types.
void destroyHgiBuffer(pxr::HgiBufferHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiResourceBindings(pxr::HgiResourceBindingsHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiComputePipeline(pxr::HgiComputePipelineHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiAccelerationStructureGeometry(
    pxr::HgiAccelerationStructureGeometryHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiAccelerationStructure(pxr::HgiAccelerationStructureHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiTexture(pxr::HgiTextureHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiSampler(pxr::HgiSamplerHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiShaderFunction(pxr::HgiShaderFunctionHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiShaderProgram(pxr::HgiShaderProgramHandle* pHdl, pxr::Hgi* pHgi);
void destroyHgiRayTracingPipeline(pxr::HgiRayTracingPipelineHandle* pHdl, pxr::Hgi* pHgi);

// Alias wrapper classes for all the HGI handle types.
using HgiBufferHandleWrapper = HgiHandleWrapper<pxr::HgiBufferHandle, destroyHgiBuffer>;
using HgiResourceBindingsHandleWrapper =
    HgiHandleWrapper<pxr::HgiResourceBindingsHandle, destroyHgiResourceBindings>;
using HgiComputePipelineHandleWrapper =
    HgiHandleWrapper<pxr::HgiComputePipelineHandle, destroyHgiComputePipeline>;
using HgiAccelerationStructureGeometryHandleWrapper =
    HgiHandleWrapper<pxr::HgiAccelerationStructureGeometryHandle,
        destroyHgiAccelerationStructureGeometry>;
using HgiAccelerationStructureHandleWrapper =
    HgiHandleWrapper<pxr::HgiAccelerationStructureHandle, destroyHgiAccelerationStructure>;
using HgiTextureHandleWrapper = HgiHandleWrapper<pxr::HgiTextureHandle, destroyHgiTexture>;
using HgiSamplerHandleWrapper = HgiHandleWrapper<pxr::HgiSamplerHandle, destroyHgiSampler>;
using HgiShaderFunctionHandleWrapper =
    HgiHandleWrapper<pxr::HgiShaderFunctionHandle, destroyHgiShaderFunction>;
using HgiShaderProgramHandleWrapper =
    HgiHandleWrapper<pxr::HgiShaderProgramHandle, destroyHgiShaderProgram>;
using HgiRayTracingPipelineHandleWrapper =
    HgiHandleWrapper<pxr::HgiRayTracingPipelineHandle, destroyHgiRayTracingPipeline>;

END_AURORA
