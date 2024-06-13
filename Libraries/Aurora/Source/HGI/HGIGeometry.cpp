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

#include "HGIScene.h"

#include "CompiledShaders/HGIShaders.h"
#include "HGIImage.h"
#include "HGIMaterial.h"
#include "HGIRenderBuffer.h"
#include "HGIRenderer.h"

using namespace pxr;

BEGIN_AURORA

HGIGeometry::HGIGeometry(
    HGIRenderer* pRenderer, const string& name, const GeometryDescriptor& geomData) :
    GeometryBase(name, geomData), _pRenderer(pRenderer)
{
}

void HGIGeometry::update()
{
    // Create sequential indices if none provided.
    if (_indices.empty())
    {
        // Create sequential index data.
        _indices.reserve(_vertexCount);
        for (uint32_t i = 0; i < _vertexCount; i++)
        {
            _indices.push_back(i);
        }
        _indexCount = _vertexCount;
    }

    // Create a vertex buffer for geometry.
    HgiBufferDesc vertexBufferDesc;
    vertexBufferDesc.debugName = _name + "VertexBuffer";
    vertexBufferDesc.usage     = HgiBufferUsageStorage |
        HgiBufferUsageAccelerationStructureBuildInputReadOnly | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    vertexBufferDesc.initialData  = _positions.data();
    vertexBufferDesc.vertexStride = sizeof(float) * 3;
    vertexBufferDesc.byteSize     = _vertexCount * vertexBufferDesc.vertexStride;
    vertexBuffer                  = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(vertexBufferDesc), _pRenderer->hgi());

    // Create a normal buffer for geometry.
    HgiBufferDesc normalBufferDesc;
    normalBufferDesc.debugName = _name + "NormalBuffer";
    normalBufferDesc.usage     = HgiBufferUsageStorage |
        HgiBufferUsageAccelerationStructureBuildInputReadOnly | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    normalBufferDesc.initialData  = _normals.data();
    normalBufferDesc.vertexStride = sizeof(float) * 3;
    normalBufferDesc.byteSize     = _vertexCount * normalBufferDesc.vertexStride;
    normalBuffer                  = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(normalBufferDesc), _pRenderer->hgi());

    // Create a tex coord buffer for geometry.
    HgiBufferDesc texCoordBufferDesc;
    texCoordBufferDesc.debugName = _name + "TexCoordBuffer";
    texCoordBufferDesc.usage     = HgiBufferUsageStorage |
        HgiBufferUsageAccelerationStructureBuildInputReadOnly | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    texCoordBufferDesc.initialData  = _texCoords.data();
    texCoordBufferDesc.vertexStride = sizeof(float) * 2;
    texCoordBufferDesc.byteSize     = _vertexCount * texCoordBufferDesc.vertexStride;
    texCoordBuffer                  = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(texCoordBufferDesc), _pRenderer->hgi());

    // Create an index buffer for geometry.
    HgiBufferDesc indexBufferDesc;
    indexBufferDesc.debugName = _name + "IndexBuffer";
    indexBufferDesc.usage     = HgiBufferUsageStorage |
        HgiBufferUsageAccelerationStructureBuildInputReadOnly | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    indexBufferDesc.initialData = _indices.data();
    indexBufferDesc.byteSize    = _indexCount * sizeof(_indices[0]);
    indexBuffer                 = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(indexBufferDesc), _pRenderer->hgi());
    
    // Per primitive data
    HgiBufferDesc primitiveDataBufferDesc;
    primitiveDataBufferDesc.debugName = _name + "PrimitiveData";
    primitiveDataBufferDesc.usage     = HgiBufferUsageStorage |
        HgiBufferUsageAccelerationStructureBuildInputReadOnly | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    primitiveDataBufferDesc.initialData  = _primitiveData.data();
    primitiveDataBufferDesc.vertexStride = sizeof(Triangle);
    primitiveDataBufferDesc.byteSize     = (_indexCount / 3) * primitiveDataBufferDesc.vertexStride;
    primitiveDataBuffer                  = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(primitiveDataBufferDesc), _pRenderer->hgi());

    // Get the device addresses for all the buffers to place in shader record.
    bufferAddresses.indexBufferDeviceAddress    = indexBuffer->handle()->GetDeviceAddress();
    bufferAddresses.vertexBufferDeviceAddress   = vertexBuffer->handle()->GetDeviceAddress();
    bufferAddresses.normalBufferDeviceAddress   = normalBuffer->handle()->GetDeviceAddress();
    bufferAddresses.texCoordBufferDeviceAddress = texCoordBuffer->handle()->GetDeviceAddress();

    // Create an identity transform matrix buffer (this is for the geometry itself, not the
    // instance.)
    float transformMatrix[] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.0f };
    HgiBufferDesc transformBufferDesc;
    transformBufferDesc.debugName = _name + "GeometryTransformBuffer";
    transformBufferDesc.usage     = HgiBufferUsageAccelerationStructureBuildInputReadOnly |
        HgiBufferUsageRayTracingExtensions | HgiBufferUsageShaderDeviceAddress;
    transformBufferDesc.initialData = &transformMatrix;
    transformBufferDesc.byteSize    = 1 * sizeof(transformMatrix);
    transformBuffer                 = HgiBufferHandleWrapper::create(
        _pRenderer->hgi()->CreateBuffer(transformBufferDesc), _pRenderer->hgi());

    // create the BLAS triangle geometry description for this geometry.
    uint32_t triangleCount = _indexCount / 3;
    HgiAccelerationStructureTriangleGeometryDesc geometryDesc;
    geometryDesc.debugName     = "Bottom Level Geometry";
    geometryDesc.indexData     = indexBuffer->handle();
    geometryDesc.vertexData    = vertexBuffer->handle();
    geometryDesc.vertexStride  = vertexBufferDesc.vertexStride;
    geometryDesc.maxVertex     = _vertexCount;
    geometryDesc.transformData = transformBuffer->handle();
    geometryDesc.count         = triangleCount;
    geometryDesc.primitiveData              = primitiveDataBuffer->handle();
    geometryDesc.primitiveDataStride        = primitiveDataBufferDesc.vertexStride;
    geometryDesc.primitiveDataElementSize   = primitiveDataBufferDesc.vertexStride;

    geom                       = HgiAccelerationStructureGeometryHandleWrapper::create(
        _pRenderer->hgi()->CreateAccelerationStructureGeometry(geometryDesc), _pRenderer->hgi());

    // create the BLAS description for this geometry.
    HgiAccelerationStructureDesc asDesc;
    asDesc.debugName = "Bottom Level AS";
    asDesc.geometry.push_back(geom->handle());
    asDesc.type    = HgiAccelerationStructureTypeBottomLevel;
    accelStructure = HgiAccelerationStructureHandleWrapper::create(
        _pRenderer->hgi()->CreateAccelerationStructure(asDesc), _pRenderer->hgi());

    // Build the BLAS for this geometry.
    HgiAccelerationStructureCmdsUniquePtr accelStructCmds =
        _pRenderer->hgi()->CreateAccelerationStructureCmds();
    accelStructCmds->PushDebugGroup("Build BLAS cmds");
    accelStructCmds->Build({ accelStructure->handle() }, { { (uint32_t)geometryDesc.count } });
    accelStructCmds->PopDebugGroup();

    // Buld the BLAS.
    _pRenderer->hgi()->SubmitCmds(accelStructCmds.get(), HgiSubmitWaitTypeWaitUntilCompleted);
}

END_AURORA
