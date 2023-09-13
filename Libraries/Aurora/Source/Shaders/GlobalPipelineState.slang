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

// Prefix containing the common code used by all material types.
#ifndef GLOBALPIPELINESTATE_H
#define GLOBALPIPELINESTATE_H

#include "Geometry.slang"
#include "Frame.slang"

// =================================================================================================
// Structure defintions, used in global pipeline state
// =================================================================================================

// Layout of environment constant properties.
struct EnvironmentConstants
{
    float3 lightTop;
    float _padding1;
    float3 lightBottom;
    float lightTexLuminanceIntegral;
    float4x4 lightTransform;
    float4x4 lightTransformInv;
    float3 backgroundTop;
    float _padding3;
    float3 backgroundBottom;
    float _padding4;
    float4x4 backgroundTransform;
    bool backgroundUseScreen;
    bool hasLightTex;
    bool hasBackgroundTex;
};

// Layout of an alias map entry.
// Must match CPU struct Entry in AliasMap.h.
// NOTE: Padding to 16 byte (float4) alignment is added for best performance.
struct AliasEntry
{
    uint alias;
    float probability;
    float pdf;
    float _padding1;
};

// Layout of ground plane properties.
// Must match CPU struct GroundPlaneData in PTGroundPlane.h.
struct GroundPlane
{
    bool enabled;
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float shadowOpacity;
    float3 shadowColor;
    float reflectionOpacity;
    float3 reflectionColor;
    float reflectionRoughness;
};

// =================================================================================================
// Global Variables - For All Shaders
// Must match global root signature defined defined in PTShaderLibrary::initRootSignatures and data 
// setup in PTRenderer::submitRayDispatch.
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

// Material properties for all scene materials are stored in this ByteAddressBuffer, and accessed with Material_XXX()
// functions, specific to the material type, using the offset included in the instance data for each ray hit.
ByteAddressBuffer gGlobalMaterialConstants : register(t5);

// Material textures for all the scene materials.  Looked up using indices stored in material header.
Texture2D<float4> gGlobalMaterialTextures[] : register(t6);

// The global sampler state, used by default for texture sampling.
[[vk::binding(6)]] SamplerState gDefaultSampler : register(s0);


#endif // PATHTRACINGCOMMON_H
