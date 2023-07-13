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

#include "GeometryBase.h"
#include "HGIHandleWrapper.h"

BEGIN_AURORA

// Forward declarations.
class HGIRenderer;

// The geometry buffer structure, must match GPU layout in
// Libraries\Aurora\Source\HGI\Shaders\InstanceData.glsl
struct HGIGeometryBuffers
{
    uint64_t indexBufferDeviceAddress    = 0ull;
    uint64_t vertexBufferDeviceAddress   = 0ull;
    uint64_t normalBufferDeviceAddress   = 0ull;
    uint64_t tangentBufferDeviceAddress  = 0ull;
    uint64_t texCoordBufferDeviceAddress = 0ull;
};

class HGIGeometry : public GeometryBase
{
public:
    HGIGeometry(HGIRenderer* pRenderer, const string& name, const GeometryDescriptor& geomData);
    ~HGIGeometry() {}

    void update();

    HgiBufferHandleWrapper::Pointer vertexBuffer;
    HgiBufferHandleWrapper::Pointer normalBuffer;
    HgiBufferHandleWrapper::Pointer texCoordBuffer;
    HgiBufferHandleWrapper::Pointer indexBuffer;
    HgiBufferHandleWrapper::Pointer transformBuffer;
    HgiAccelerationStructureGeometryHandleWrapper::Pointer geom;
    HgiAccelerationStructureHandleWrapper::Pointer accelStructure;

    HGIGeometryBuffers bufferAddresses;
    HGIRenderer* _pRenderer;
};

END_AURORA