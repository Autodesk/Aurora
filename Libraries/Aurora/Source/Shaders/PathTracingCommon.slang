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

// Prefix containing the common code used by all material types.
// NOTE: Does not contain a hit shader. Hit shaders for the different material types must be
// appended to this file.

// Define this symbol for NRD.
#define COMPILER_DXC

#include "BSDFCommon.slang"
#include "Colors.slang"
#include "Environment.slang"
#include "Frame.slang"
#include "Geometry.slang"
#include "Globals.slang"
#include "GroundPlane.slang"
#include "Material.slang"
#include "Random.slang"
#include "RayTrace.slang"
#include "Sampling.slang"

// =================================================================================================
// Global Variables - For All Shaders
// =================================================================================================

// The top-level acceleration structure with the scene contents.
[[vk::binding(0)]] RaytracingAccelerationStructure gScene : register(t0);

// Constant buffers of sample and per-frame values.
[[vk::binding(4)]] ConstantBuffer<SampleData> gSampleData : register(b0);
[[vk::binding(2)]] ConstantBuffer<FrameData> gFrameData : register(b1);

// Environment data.
[[vk::binding(5)]] ConstantBuffer<EnvironmentConstants> gEnvironmentConstants : register(b2);
StructuredBuffer<AliasEntry> gEnvironmentAliasMap : register(t1);
[[vk::binding(8)]] Texture2D<float4> gEnvironmentLightTexture : register(t2);
[[vk::binding(7)]] Texture2D<float4> gEnvironmentBackgroundTexture : register(t3);
ConstantBuffer<GroundPlane> gGroundPlane : register(b3);
RaytracingAccelerationStructure gNullScene : register(t4);

// =================================================================================================
// Ray Gen Shader Variables
// =================================================================================================

// The output texture.
[[vk::binding(1)]] RWTexture2D<float4> gDirect : register(u0);
RWTexture2D<float> gDepthNDC : register(u1);
RWTexture2D<float> gDepthView : register(u2);
RWTexture2D<float4> gNormalRoughness : register(u3);
RWTexture2D<float4> gBaseColorMetalness : register(u4);
RWTexture2D<float4> gDiffuse : register(u5);
RWTexture2D<float4> gGlossy : register(u6);

// =================================================================================================
// Radiance Hit Shader Variables
// =================================================================================================

// The maximum number of supported material layers, must match value in
// PTLayerIndexTable::kMaxMaterialLayers C++ code.
#define kMaxMaterialLayers 64

// The layer material shader IDs.
struct MaterialLayerShaderIDs
{
    // Shader indices (must be stored as floattors due to HLSL packing rules).
    int4 shaderIDs[kMaxMaterialLayers / WORD_SIZE];
};

#if DIRECTX
// Geometry data, for the current instance only.
ByteAddressBuffer gIndices : register(t0, space1);
ByteAddressBuffer gPositions : register(t1, space1);
ByteAddressBuffer gNormals : register(t2, space1);
ByteAddressBuffer gTexCoords : register(t3, space1);

// NOTE: Material variables are inserted at register(b1, space1) between gGeometryMetadata and
// gMaterialLayerIDs by Material.slang.
cbuffer gGeometryMetadata : register(b0, space1)
{
    bool gHasNormals;        // Are there normals?
    bool gHasTexCoords;      // Are there texture coordinates?
    int gMaterialLayerCount; // Number of material layer miss shaders.
}

// To hide DX-Vulkan differences, expose geometry access using functions.
uint3 getIndicesForTriangle(int triangleIndex) {
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

float2 getTexCoordForVertex(int vertexIndex)
{
    return asfloat(gTexCoords.Load2(vertexIndex * 2 * 4));
}

bool instanceHasNormals()
{
    return gHasNormals;
}
bool instanceHasTexCoords()
{
    return gHasTexCoords;
}

#else

// Forward declare these functions on Vulkan GLSL.  As we need a platform-specific suffix file containing the implementation of these functions.
uint3 getIndicesForTriangle(int bufferLocation);
float3 getPositionForVertex(int bufferLocation);
float3 getNormalForVertex(int bufferLocation);
float2 getTexCoordForVertex(int bufferLocation);
bool instanceHasNormals();
bool instanceHasTexCoords();

#endif

// Slang interface implementation to handle geometry access in platform-independent way.
struct Geometry : IGeometry
{
    uint3 getIndices(int triangleIndex) { return getIndicesForTriangle(triangleIndex); }
    float3 getPosition(int vertexIndex) { return getPositionForVertex(vertexIndex); }
    float3 getNormal(int vertexIndex) { return normalize(getNormalForVertex(vertexIndex)); }
    float2 getTexCoord(int vertexIndex) { return getTexCoordForVertex(vertexIndex); }
    bool hasTexCoords() { return instanceHasTexCoords(); }
    bool hasNormals() { return instanceHasNormals(); }
} gGeometry;

// Constant buffer for layer material shader IDs.
ConstantBuffer<MaterialLayerShaderIDs> gMaterialLayerIDs : register(b2, space1);

// Get the layer material index for given layer.
int getMaterialLayerIndex(int layer)
{
    // TODO: Optimize the floattor->scalar look-up.
    return gMaterialLayerIDs.shaderIDs[layer / 4][layer % 4];
}

// =================================================================================================
// Utility Functions
// =================================================================================================

// Collects the full set of property values for an environment.
Environment prepareEnvironmentValues()
{
    Environment values;
    values.constants = gEnvironmentConstants;
    values.backgroundTexture = gEnvironmentBackgroundTexture;
    values.sampler           = gDefaultSampler;
    values.lightTexture      = gEnvironmentLightTexture;
#if DIRECTX
    values.aliasMap          = gEnvironmentAliasMap;
#endif
    return values;
}

// Adjusts the specified radiance, to clamp extreme values and detect errors.
void adjustRadiance(float maxLuminance, bool displayErrors, inout float3 radiance)
{
    // Clamp result colors above a certain luminance threshold, to minimize fireflies.
    // NOTE: This biases the final result and should be used carefully.
    float lum = luminance(radiance);
    if (lum > maxLuminance)
    {
        radiance *= maxLuminance / lum;
    }

    // Replace an invalid radiance sample with a diagnostic (infinite) value when displaying errors,
    // or black otherwise. Shading errors are usually the result of bad geometry (e.g. zero-length
    // normals), but may also be caused by internal shading errors that should be addressed.
    static const float3 NAN_COLOR = float3(INFINITY, 0.0f, 0.0f); // red
    static const float3 INF_COLOR = float3(0.0f, INFINITY, 0.0f); // green
    if (any(isnan(radiance)))
    {
        radiance = displayErrors ? NAN_COLOR : BLACK;
    }
    else if (any(isinf(radiance)))
    {
        radiance = displayErrors ? INF_COLOR : BLACK;
    }
}