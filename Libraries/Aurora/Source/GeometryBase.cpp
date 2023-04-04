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

#include "GeometryBase.h"

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
        memcpy(dst.data(), pSrc, bufferSize);
    }
    else
    {
        // If it doesn't match the input must be interleaved in some way. So iterate through each
        // vertex.
        ComponentType* pDstComp = dst.data();
        for (size_t i = 0; i < vertexCount; i++)
        {
            // Copy the individual element from the soure buffer to destination.
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

GeometryBase::GeometryBase(const std::string& name, const GeometryDescriptor& descriptor) :
    _name(name)
{
    // Position values must be provided, there must be at least three vertices. If indices are
    // provided, there must be at least three of them. All other data is optional.
    AU_ASSERT(descriptor.vertexDesc.count >= 3, "Invalid vertex data");

    _incomplete = !descriptor.vertexDesc.hasAttribute(Names::VertexAttributes::kPosition);

    // Ensure correct index count.  Must either not have any indices or have three or more of them.
    AU_ASSERT(descriptor.indexCount == 0 || descriptor.indexCount >= 3, "Invalid index data");

    if (!_incomplete)
    {
        auto positionAttributeType =
            descriptor.vertexDesc.attributes.at(Names::VertexAttributes::kPosition);
        AU_ASSERT(positionAttributeType == AttributeFormat::Float3,
            "Unsupported type for position attribute: %d", positionAttributeType);
    }

    // Get vertex and index count.
    _vertexCount = static_cast<uint32_t>(descriptor.vertexDesc.count);
    _indexCount  = static_cast<uint32_t>(descriptor.indexCount);

    // Run the getAttributeData function to get the vertex and index data from client.
    AttributeDataMap vertexBuffers;
    descriptor.getAttributeData(vertexBuffers, 0, _vertexCount, 0, _indexCount);

    // Copy the vertex and index data.
    copyVertexChannelData(
        _positions, vertexBuffers[Names::VertexAttributes::kPosition], _vertexCount, 3);
    copyVertexChannelData(
        _normals, vertexBuffers[Names::VertexAttributes::kNormal], _vertexCount, 3);
    copyVertexChannelData(
        _tangents, vertexBuffers[Names::VertexAttributes::kTangent], _vertexCount, 3);
    copyVertexChannelData(
        _texCoords, vertexBuffers[Names::VertexAttributes::kTexCoord0], _vertexCount, 2);
    copyVertexChannelData(_indices, vertexBuffers[Names::VertexAttributes::kIndices], _indexCount);

    // Run the optional attributeUpdateComplete functoin to free any buffers being held by the
    // client.
    if (descriptor.attributeUpdateComplete)
        descriptor.attributeUpdateComplete(vertexBuffers, 0, _vertexCount, 0, _indexCount);

    // TODO: We need a better UV generator, esspecially if we need tangents generated from them.
    if (_texCoords.size() == 0)
    {
        _texCoords.resize(_vertexCount * 2);
        for (uint32_t i = 0; i < _vertexCount; ++i)
        {
            float coord             = static_cast<float>(i) / _vertexCount;
            _texCoords[i * 2u]      = coord;
            _texCoords[i * 2u + 1u] = coord;
        }
    }
}

END_AURORA
