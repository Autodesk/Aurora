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
#ifndef __INSTANCEDATA_H__
#define __INSTANCEDATA_H__

// =================================================================================================
// Instance pipeline state.
// Must match CPU version in HitGroupShaderRecord in PTScene.cpp.
// =================================================================================================


#if DIRECTX
// Geometry data, for the current instance only.
ByteAddressBuffer gIndices : register(t0, space1);
ByteAddressBuffer gPositions : register(t1, space1);
ByteAddressBuffer gNormals : register(t2, space1);
ByteAddressBuffer gTangents : register(t3, space1);
ByteAddressBuffer gTexCoords : register(t4, space1);

// The data for instance and its associate geometry and material. 
// Must match CPU struct HitGroupShaderRecord in PTScene.cpp.
cbuffer gInstanceData : register(b0, space1)
{
    bool gHasNormals;           // Are there normals?
    bool gHasTangents;         // Are there tangents?
    bool gHasTexCoords;        // Are there texture coordinates?
    bool gIsOpaque;            // Is the geometry opaque?
    int gInstanceBufferOffset; // Offset of instance's data within global instance byte buffer.
}

// To hide DX-Vulkan differences, expose geometry access using functions.
uint3 getIndicesForTriangle(int triangleIndex)
{
    return gIndices.Load3((triangleIndex * 3) * 4);
}

float3 getPositionForVertex(int vertexIndex)
{
    return asfloat(gPositions.Load3(vertexIndex * 3 * 4));
}

float3 getNormalForVertex(int vertexIndex)
{
    return asfloat(gNormals.Load3(vertexIndex * 3 * 4));
}

float3 getTangentForVertex(int vertexIndex)
{
    return asfloat(gTangents.Load3(vertexIndex * 3 * 4));
}

float2 getTexCoordForVertex(int vertexIndex)
{
    return asfloat(gTexCoords.Load2(vertexIndex * 2 * 4));
}

bool instanceHasNormals()
{
    return gHasNormals;
}
bool instanceHasTangents()
{
    return gHasTangents;
}
bool instanceHasTexCoords()
{
    return gHasTexCoords;
}

#else

// Forward declare these functions on Vulkan GLSL.  As we need a platform-specific suffix file
// containing the implementation of these functions.
uint3 getIndicesForTriangle(int bufferLocation);
float3 getPositionForVertex(int bufferLocation);
float3 getNormalForVertex(int bufferLocation);
float3 getTangentForVertex(int bufferLocation);
float2 getTexCoordForVertex(int bufferLocation);
bool instanceHasNormals();
bool instanceHasTangents();
bool instanceHasTexCoords();

#endif

// Slang interface implementation to handle geometry access in platform-independent way.
struct Geometry : IGeometry
{
    uint3 getIndices(int triangleIndex) { return getIndicesForTriangle(triangleIndex); }
    float3 getPosition(int vertexIndex) { return getPositionForVertex(vertexIndex); }
    float3 getNormal(int vertexIndex) { return getNormalForVertex(vertexIndex); }
    float3 getTangent(int vertexIndex) { return getTangentForVertex(vertexIndex); }
    float2 getTexCoord(int vertexIndex) { return getTexCoordForVertex(vertexIndex); }
    bool hasTexCoords() { return instanceHasTexCoords(); }
    bool hasNormals() { return instanceHasNormals(); }
    bool hasTangents() { return instanceHasTangents(); }
} gGeometry;

#endif
