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

#include "CompiledShaders/HGIShaders.h"
#include "HGIEnvironment.h"
#include "HGIGroundPlane.h"
#include "HGIImage.h"
#include "HGIMaterial.h"
#include "HGIRenderBuffer.h"
#include "HGIRenderer.h"
#include "HGIScene.h"
#include "HGIWindow.h"

using namespace pxr;

BEGIN_AURORA

// Implementations for all the DestroyFunction types.

void destroyHgiBuffer(pxr::HgiBufferHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyBuffer(pHdl);
}

void destroyHgiResourceBindings(pxr::HgiResourceBindingsHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyResourceBindings(pHdl);
}

void destroyHgiComputePipeline(pxr::HgiComputePipelineHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyComputePipeline(pHdl);
}

void destroyHgiAccelerationStructureGeometry(
    pxr::HgiAccelerationStructureGeometryHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyAccelerationStructureGeometry(pHdl);
}

void destroyHgiAccelerationStructure(pxr::HgiAccelerationStructureHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyAccelerationStructure(pHdl);
}

void destroyHgiTexture(pxr::HgiTextureHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyTexture(pHdl);
}

void destroyHgiSampler(pxr::HgiSamplerHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroySampler(pHdl);
}

void destroyHgiShaderFunction(pxr::HgiShaderFunctionHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyShaderFunction(pHdl);
}

void destroyHgiShaderProgram(pxr::HgiShaderProgramHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyShaderProgram(pHdl);
}

void destroyHgiRayTracingPipeline(pxr::HgiRayTracingPipelineHandle* pHdl, pxr::Hgi* pHgi)
{
    pHgi->DestroyRayTracingPipeline(pHdl);
}

END_AURORA
