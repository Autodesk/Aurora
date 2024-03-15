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

#include "MemoryPool.h"

#include "PTRenderer.h"

BEGIN_AURORA

uint8_t* TransferBuffer::map(size_t sz, size_t offset)
{
    // Ensure buffer is not already mapped.
    AU_ASSERT(!isMapped(), "Already mapped");

    // Set the mapped range from size and offset.
    mappedRange          = { offset, sz };
    isUploadBufferMapped = true;

    // Map the DX buffer resource, call AU_FAIL if HRESULT indicates an error.
    uint8_t* pMappedData = nullptr;
    checkHR(pUploadBuffer->Map(
        0, sz == 0 ? nullptr : &mappedRange, reinterpret_cast<void**>(&pMappedData)));

    // Return mapped pointer.
    return pMappedData;
}

void TransferBuffer::unmap()
{
    // Ensure buffer is mapped.
    AU_ASSERT(isMapped(), "Not mapped");

    // Unmap the DX buffer resource for the upload buffer.
    pUploadBuffer->Unmap(0, mappedRange.End == 0 ? nullptr : &mappedRange); // no HRESULT

    isUploadBufferMapped = false;

    // Pass this buffer to renderer to add to the pending upload list.
    pRenderer->transferBufferUpdated(*this);

    // Reset the mapped range to invalid values.
    mappedRange = { SIZE_MAX, SIZE_MAX };
}

END_AURORA
