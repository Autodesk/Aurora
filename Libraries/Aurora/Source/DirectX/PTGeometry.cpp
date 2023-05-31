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

#include "PTGeometry.h"

#include "PTRenderer.h"

BEGIN_AURORA

// Copies data from the specified array to the specified vector, with each element having a
// specified number of components, e.g. XYZ for a vertex position.
template <typename ComponentType>
static void copyVertexChannelData(vector<ComponentType>& dst, const AttributeData& src,
    uint32_t vertexCount, uint32_t componentCount = 1)
{
    // If no source data is provided, clear the destination vector and return.
    if (!src.address)
    {
        dst.clear();
        return;
    }

    // Get start of source buffer.
    const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(src.address) + src.offset;

    // Set the size of the vector with enough elements to include the vertex data, based on the
    // specified data type (e.g. float) and number of components per vertex (e.g. three for a
    // position, XYZ).
    size_t bufferSize = vertexCount * componentCount * sizeof(ComponentType);
    dst.resize(bufferSize);

    // If the stride of the source data is just the size of the element, just do a memcpy.
    if (src.stride == componentCount * sizeof(ComponentType))
    {
        ::memcpy_s(dst.data(), bufferSize, pSrc, bufferSize);
    }
    else
    {
        // If it doesn't match the input must be interleaved in some way. So iterate through each
        // vertex.
        ComponentType* pDstComp = dst.data();
        for (size_t i = 0; i < vertexCount; i++)
        {
            // Copy the individual element from the source buffer to destination.
            const ComponentType* pSrcComp = reinterpret_cast<const ComponentType*>(pSrc);
            for (uint32_t j = 0; j < componentCount; j++)
            {
                pDstComp[j] = pSrcComp[j];
            }

            // Advance the destination pointer by size of element.
            pDstComp += componentCount;

            // Advance the source pointer by the stride provided by client.
            pSrc += src.stride;
        }
    }
}

PTGeometry::PTGeometry(
    PTRenderer* pRenderer, const string& name, const GeometryDescriptor& descriptor) :
    GeometryBase(name, descriptor), _pRenderer(pRenderer)
{
}

bool PTGeometry::update()
{
    // Do nothing if the geometry is not dirty.
    if (!_bIsDirty)
    {
        return false;
    }

    // Create an index buffer from the index data. If there is no index data, create simple index
    // data with sequential values: 0, 1, 2, ...
    // NOTE: Index data is optional for the geometry, but required for shaders, which is why a
    // buffer exist at this point.
    if (_indices.empty())
    {
        // Create sequential index data.
        vector<uint32_t> indices;
        indices.reserve(_vertexCount);
        for (uint32_t i = 0; i < _vertexCount; i++)
        {
            indices.push_back(i);
        }

        // Create the index buffer from the sequential index data.
        createVertexBuffer(_indexBuffer, indices.data(), sizeof(uint32_t) * _vertexCount);
    }
    else
    {
        // Create the index buffer from the existing index data.
        createVertexBuffer(_indexBuffer, _indices.data(), sizeof(uint32_t) * _indexCount);
    }

    // Create vertex buffers, one for each channel of data: positions (required), normals
    // (optional), and texture coordinates (optional).
    if (!_positions.empty())
    {
        createVertexBuffer(_positionBuffer, _positions.data(), sizeof(float) * _vertexCount * 3);
    }
    if (!_normals.empty())
    {
        createVertexBuffer(_normalBuffer, _normals.data(), sizeof(float) * _vertexCount * 3);
    }
    if (!_tangents.empty())
    {
        createVertexBuffer(_tangentBuffer, _tangents.data(), sizeof(float) * _vertexCount * 3);
    }
    if (!_texCoords.empty())
    {
        createVertexBuffer(_texCoordBuffer, _texCoords.data(), sizeof(float) * _vertexCount * 2);
    }

    // Reset the BLAS pointer as it will have to be rebuilt. Also clear the dirty flag.
    _pBLAS.Reset();
    _bIsDirty = false;

    return true;
}

bool PTGeometry::updateBLAS()
{
    // Build the bottom-level acceleration structure (BLAS) if it doesn't exist.
    // NOTE: The BLAS used to later build a TLAS must be retained somewhere (not released), even
    // after the TLAS is created, and even if the BLAS are not used directly by the application. Not
    // doing this results in the device being lost (crash), without feedback.
    if (!_pBLAS)
    {
        _pBLAS = buildBLAS();

        return true;
    }

    return false;
}

void PTGeometry::createVertexBuffer(VertexBuffer& vertexBuffer, void* pData, size_t dataSize) const
{
    _pRenderer->getVertexBuffer(vertexBuffer, pData, dataSize);
}

ID3D12ResourcePtr PTGeometry::buildBLAS()
{
    // Describe the geometry for the BLAS.
    // NOTE: The geometry is not set as opaque here on the geometry flags, so that the any hit
    // shader can be called if needed. If the any hit shader is not needed, the shader will use the
    // opaque ray flag when calling TraceRay().
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    auto& triangles                             = geometryDesc.Triangles;
    geometryDesc.Type                           = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags                          = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

    // Specify the vertex data.
    triangles.VertexCount                = _vertexCount;
    triangles.VertexFormat               = DXGI_FORMAT_R32G32B32_FLOAT;
    triangles.VertexBuffer.StartAddress  = _positionBuffer.gpuAddress();
    triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;

    // Specify the index data, if any.
    if (!_indices.empty())
    {
        triangles.IndexCount  = _indexCount;
        triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        triangles.IndexBuffer = _indexBuffer.gpuAddress();
    }

    // Describe the BLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blasInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    blasInputs.NumDescs       = 1;
    blasInputs.pGeometryDescs = &geometryDesc;

    // Get the sizes required for the BLAS scratch and result buffers, and create them.
    // NOTE: The scratch buffer is obtained from the renderer and will be retained by the renderer
    // for the duration of the build task started below, and then it will be released. These are
    // typically around 50 KB in size, depending on the represented geometry.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo = {};
    _pRenderer->dxDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);
    D3D12_GPU_VIRTUAL_ADDRESS blasScratchAddress =
        _pRenderer->getScratchBuffer(blasInfo.ScratchDataSizeInBytes);
    ID3D12ResourcePtr pBLAS = _pRenderer->createBuffer(blasInfo.ResultDataMaxSizeInBytes,
        "BLAS Buffer", D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    // Describe the build for the BLAS.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
    blasDesc.Inputs                                             = blasInputs;
    blasDesc.ScratchAccelerationStructureData                   = blasScratchAddress;
    blasDesc.DestAccelerationStructureData                      = pBLAS->GetGPUVirtualAddress();

    // Build the BLAS using a command list. Insert a UAV barrier so that it can't be used until it
    // is generated.
    ID3D12GraphicsCommandList4Ptr pCommandList = _pRenderer->beginCommandList();
    pCommandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);
    _pRenderer->addUAVBarrier(pBLAS.Get());

    // Complete the command list and task.
    // NOTE: It is not necessary to wait for the task to complete because of the UAV barrier added
    // above. Also, the scratch buffer will be retained by the renderer for the duration of the
    // task, because it was obtained from the renderer with getScratchBuffer().
    _pRenderer->submitCommandList();
    _pRenderer->completeTask();

    return pBLAS;
}

END_AURORA
