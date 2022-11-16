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

#include "PTGeometry.h"

BEGIN_AURORA

// A pool used to supply scratch buffers for the creation of BLAS and TLAS.
// NOTE: This improves performance in two ways:
// - By retaining scratch buffers for the current task, so that the build function doesn't need to
//   manually wait for the GPU to finish before releasing the buffers.
// - By allocating small scratch buffers from a single larger buffer, to reduce the number of
//   actual allocations required.
class ScratchBufferPool
{
public:
    using FuncCreateScratchBuffer = function<ID3D12ResourcePtr(size_t)>;

    // Constructor. This accepts the number of simultaneous tasks and a function to create a single
    // GPU scratch buffer.
    ScratchBufferPool(uint32_t taskCount, FuncCreateScratchBuffer funcCreateScratchBuffer)
    {
        assert(funcCreateScratchBuffer);

        // Initialize the array of task buffers, i.e. one list of buffers for each simultaneously
        // active task.
        _taskBuffers.resize(taskCount);

        // Create the initial buffer.
        _funcCreateScratchBuffer = funcCreateScratchBuffer;
        _pBuffer                 = _funcCreateScratchBuffer(kBufferSize);
    }

    // Gets the GPU address of a scratch buffer with the specified size. This scratch buffer is only
    // guaranteed to be valid until the next task.
    D3D12_GPU_VIRTUAL_ADDRESS get(size_t size)
    {
        // If the requested size exceeds the total buffer size, just create a dedicated buffer and
        // add it the list of buffers for the current task.
        if (size > kBufferSize)
        {
            ID3D12ResourcePtr pLargeBuffer = _funcCreateScratchBuffer(size);
            _taskBuffers[_taskIndex].push_back(pLargeBuffer);

            return pLargeBuffer->GetGPUVirtualAddress();
        }

        // If the current buffer doesn't have enough space for the requested scratch buffer, add it
        // to the list of buffers for the current task (i.e. "retire" it) and create a new buffer.
        if (_bufferOffset + size > kBufferSize)
        {
            _taskBuffers[_taskIndex].push_back(_pBuffer);
            _pBuffer      = _funcCreateScratchBuffer(kBufferSize);
            _bufferOffset = 0;
        }

        // Get the GPU address for the current buffer offset.
        D3D12_GPU_VIRTUAL_ADDRESS address = _bufferOffset + _pBuffer->GetGPUVirtualAddress();

        // Increment the buffer offset, while taking the acceleration structure byte alignment (256)
        // into account.
        const size_t kAlignment = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;
        _bufferOffset += size;
        _bufferOffset += ALIGNED_OFFSET(_bufferOffset, kAlignment);

        return address;
    }

    // Notifies the scratch buffer pool that the current task has advanced to the specified index.
    // This means the pool can purge scratch buffers for prior tasks.
    // NOTE: This index should loop through the task count, e.g. 0, 1, 2, 0, 1, 2, etc.
    void update(uint32_t taskIndex)
    {
        assert(taskIndex < _taskBuffers.size());

        // Clear the list of buffers for the current task, which will also release the buffers.
        // NOTE: The default destructor for this class will similarly release everything.
        _taskBuffers[taskIndex].clear();
        _taskIndex = taskIndex;
    }

private:
    // The buffer size is selected to support dozens of average BLAS scratch buffers, or at least
    // one very large TLAS scratch buffer.
    const size_t kBufferSize = 4 * 1024 * 1024;

    FuncCreateScratchBuffer _funcCreateScratchBuffer;
    uint32_t _taskIndex = 0;
    vector<vector<ID3D12ResourcePtr>> _taskBuffers;
    ID3D12ResourcePtr _pBuffer;
    size_t _bufferOffset = 0;
};

// A pool used to supply vertex buffers, e.g. index, position, normal, and tex coords. This improves
// performance by collecting many small virtual allocations into fewer physical allocations of GPU
// memory.
class VertexBufferPool
{
public:
    using FuncCreateVertexBuffer = function<ID3D12ResourcePtr(size_t)>;

    // Constructor. This accepts to create a single GPU vertex buffer.
    VertexBufferPool(FuncCreateVertexBuffer funcCreateVertexBuffer)
    {
        assert(funcCreateVertexBuffer);

        // Create the initial buffer, mapped for writing.
        // NOTE: We deliberately do not specify a read range here, i.e. second parameter for Map().
        // Testing has shown that specifying the read range (zero start and end, to indicate that no
        // reading will be performed), actually results in *slower* upload in practice.
        _funcCreateVertexBuffer = funcCreateVertexBuffer;
        _pBuffer                = _funcCreateVertexBuffer(kBufferSize);
        checkHR(_pBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_pMappedData)));
    }

    // Gets a vertex buffer with the size specified in the provided vertex buffer description, and
    // fills it with the provided data. The resource pointer and buffer offset of the vertex buffer
    // description will be populated.
    void get(VertexBuffer& vertexBuffer, void* pData)
    {
        AU_ASSERT(vertexBuffer.size > 0 && pData, "Invalid vertex buffer");

        // If the requested size exceeds the total buffer size, just create a dedicated buffer and
        // and use it directly.
        size_t size = vertexBuffer.size;
        if (size > kBufferSize)
        {
            // Create the (large) buffer.
            ID3D12ResourcePtr pLargeBuffer = _funcCreateVertexBuffer(size);

            // Map it for reading and copy the provided data.
            CD3DX12_RANGE writeRange(0, size);
            uint8_t* pMappedData = nullptr;
            checkHR(pLargeBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pMappedData)));
            ::memcpy_s(pMappedData, size, pData, size);
            pLargeBuffer->Unmap(0, &writeRange); // no HRESULT

            // Update the provided vertex buffer structure.
            vertexBuffer.pBuffer = pLargeBuffer;
            vertexBuffer.offset  = 0;

            return;
        }

        // If the current buffer doesn't have enough space for the requested size, flush the pool,
        // which creates a new buffer.
        if (_bufferOffset + size > kBufferSize)
        {
            flush();
        }

        // Copy the provided data to the mapped buffer.
        ::memcpy_s(_pMappedData + _bufferOffset, size, pData, size);

        // Update the provided vertex buffer structure. By assigning the buffer resource pointer to
        // the vertex buffer, the owner of the vertex buffer takes on shared ownership of the
        // resource, so that it can safely be released by the pool later.
        vertexBuffer.pBuffer = _pBuffer;
        vertexBuffer.offset  = _bufferOffset;

        // Increment the buffer offset, while taking the acceleration structure byte alignment (256)
        // into account.
        _bufferOffset += size;
    }

    // Flushes the vertex buffer pool, so that all created vertex buffers are available to the GPU.
    // This must be done before using the vertex buffers on the GPU.
    void flush()
    {
        // Return if the buffer offset is zero, because there is no pending data to flush.
        if (_bufferOffset == 0)
        {
            return;
        }

        // Unmap the (open) buffer, and release the buffer.
        CD3DX12_RANGE writeRange(0, _bufferOffset - 1);
        _pBuffer->Unmap(0, &writeRange); // no HRESULT
        _pBuffer.Reset();
        _pMappedData = nullptr;

        // Create a new buffer, mapped for writing.
        _pBuffer      = _funcCreateVertexBuffer(kBufferSize);
        _bufferOffset = 0;
        checkHR(_pBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_pMappedData)));
    }

private:
    // The buffer size is selected to support dozens of average vertex buffers.
    const size_t kBufferSize = 4 * 1024 * 1024;

    FuncCreateVertexBuffer _funcCreateVertexBuffer;
    ID3D12ResourcePtr _pBuffer;
    uint8_t* _pMappedData = nullptr;
    size_t _bufferOffset  = 0;
};

END_AURORA