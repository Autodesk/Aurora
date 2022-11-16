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

#include "HGIEnvironment.h"

#include "HGIMaterial.h"
#include "HGIRenderer.h"

using namespace pxr;

BEGIN_AURORA

HGIEnvironment::HGIEnvironment(HGIRenderer* pRenderer) : _pRenderer(pRenderer)
{
    // Create buffer descriptor, passing material as initial data.
    HgiBufferDesc uboDesc;
    uboDesc.debugName = "Environment UBO";
    uboDesc.usage     = HgiBufferUsageUniform | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    uboDesc.byteSize = sizeof(EnvironmentData);

    // Create UBO.
    _ubo =
        HgiBufferHandleWrapper::create(_pRenderer->hgi()->CreateBuffer(uboDesc), _pRenderer->hgi());
}

void HGIEnvironment::update()
{
    // Build a structure from values map in staging buffer.
    EnvironmentData* pStaging = getStagingAddress<EnvironmentData>(_ubo);
    updateGPUStruct(*pStaging);

    // Transfer staging buffer to GPU.
    pxr::HgiBlitCmdsUniquePtr blitCmds = _pRenderer->hgi()->CreateBlitCmds();
    pxr::HgiBufferCpuToGpuOp blitOp;
    blitOp.byteSize              = sizeof(EnvironmentData);
    blitOp.cpuSourceBuffer       = pStaging;
    blitOp.sourceByteOffset      = 0;
    blitOp.gpuDestinationBuffer  = _ubo->handle();
    blitOp.destinationByteOffset = 0;
    blitCmds->CopyBufferCpuToGpu(blitOp);
    _pRenderer->hgi()->SubmitCmds(blitCmds.get());
}

END_AURORA
