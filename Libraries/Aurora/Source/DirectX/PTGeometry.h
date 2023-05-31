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

#include "GeometryBase.h"

BEGIN_AURORA

// Forward declarations.
class PTRenderer;

// A class describing a vertex buffer obtained from a vertex buffer pool.
class VertexBuffer
{
public:
    VertexBuffer(ID3D12ResourcePtr buffer = nullptr, size_t size = 0, size_t offset = 0) :
        _pGPUBuffer(buffer), _size(size), _offset(offset)
    {
    }

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const
    {
        return _pGPUBuffer ? _pGPUBuffer->GetGPUVirtualAddress() + _offset : 0;
    }
    size_t size() { return _size; }

private:
    ID3D12ResourcePtr _pGPUBuffer;
    size_t _size   = 0;
    size_t _offset = 0;
};


// An internal implementation for IGeometry.
class PTGeometry : public GeometryBase
{
public:
    /*** Types ***/

    struct GeometryBuffers
    {
        D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer    = 0;
        D3D12_GPU_VIRTUAL_ADDRESS PositionBuffer = 0;
        D3D12_GPU_VIRTUAL_ADDRESS NormalBuffer   = 0;
        D3D12_GPU_VIRTUAL_ADDRESS TangentBuffer   = 0;
        D3D12_GPU_VIRTUAL_ADDRESS TexCoordBuffer = 0;
    };

    /*** Lifetime Management ***/

    PTGeometry(PTRenderer* pRenderer, const string& name, const GeometryDescriptor& descriptor);
    ~PTGeometry() {};

    /*** Functions ***/

    GeometryBuffers buffers() const
    {
        GeometryBuffers buffers;
        buffers.IndexBuffer    = _indexBuffer.gpuAddress();
        buffers.PositionBuffer = _positionBuffer.gpuAddress();
        buffers.NormalBuffer   = _normalBuffer.gpuAddress();
        buffers.TangentBuffer  = _tangentBuffer.gpuAddress();
        buffers.TexCoordBuffer = _texCoordBuffer.gpuAddress();
        return buffers;
    }
    ID3D12Resource* blas() { return _pBLAS.Get(); }
    bool update();
    bool updateBLAS();

private:
    /*** Private Functions ***/

    void createVertexBuffer(VertexBuffer& vertexBuffer, void* pData, size_t dataSize) const;
    ID3D12ResourcePtr buildBLAS();

    /*** Private Variables ***/

    PTRenderer* _pRenderer = nullptr;

    /*** DirectX 12 Objects ***/

    ID3D12ResourcePtr _pBLAS;
    VertexBuffer _indexBuffer;
    VertexBuffer _positionBuffer;
    VertexBuffer _normalBuffer;
    VertexBuffer _tangentBuffer;
    VertexBuffer _texCoordBuffer;
};
MAKE_AURORA_PTR(PTGeometry);

END_AURORA