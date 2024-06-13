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
#ifndef __GLOBAL_BUFFER_ACCESSORS_H__
#define __GLOBAL_BUFFER_ACCESSORS_H__

#include "GlobalPipelineState.slang"

// Total size of fixed-length material header in bytes.
// Material header occurs before each material's properties in the gGlobalMaterialConstants buffer.
// NOTE: Must match size of MaterialHeader in PTScene.cpp.
#define kMaterialHeaderSize 68

#if DIRECTX
// Maxiumum number of textures per-material.
// NOTE: Must match value defined in PTScene.cpp,
#define kMaterialMaxTextures 8

// Total size of fixed-length instance header in bytes.
// Instance header occurs before the layer data in the gGlobalInstanceArray buffer.
// NOTE: Must match size of InstanceDataHeader in PTScene.cpp.
#define kInstanceDataHeaderSize (2 * 4)

// Size of a 3x4 transform matrix,
#define kTransformMatrixSize (12 * 4)

// Material properties for all scene materials are stored in this ByteAddressBuffer, and accessed with Material_XXX()
// functions, specific to the material type, using the offset included in the instance data for each ray hit.
ByteAddressBuffer gGlobalMaterialConstants : register(t5);

// Instance properties for all scene instances are stored in this ByteAddressBuffer.  This is the instance transform matrix and layer properties, if any.
ByteAddressBuffer gGlobalInstanceBuffer : register(t6);

// Instance properties for all scene instances are stored in this ByteAddressBuffer.  This is the instance transform matrix and layer properties, if any.
ByteAddressBuffer gLayerGeometryBuffer : register(t7);

// Transform matrices for all the instances.
ByteAddressBuffer gTransformMatrixBuffer : register(t8);

// Material textures for all the scene materials.  Looked up using indices stored in material header.
Texture2D<float4> gGlobalMaterialTextures[] : register(t9);

// =================================================================================================
// Instance buffer accessor functions
// =================================================================================================
// Get the material offset from instance header.
int instanceMaterialBufferOffset(int instanceOffset)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * 0);
}

// Get the layer count from instance header.
int instanceLayerCount(int instanceOffset)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * 1);
}

// Get the layer material buffer offset for Nth layer.
// Layer data is stored after the header in the instance buffer.
int instanceLayerMaterialBufferOffset(int instanceOffset, int layerIdx)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + kInstanceDataHeaderSize + 4 * (0 + layerIdx * 2));
}

// Get the layer UV buffer offset for Nth layer.
// Layer data is stored after the header in the instance buffer.
int instanceLayerUVBufferOffset(int instanceOffset, int layerIdx)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + kInstanceDataHeaderSize + 4 * (1 + layerIdx * 2));
}

// Get row-major 4x3 world transform matrix for the BLAS indicated by index
float3x4 transformMatrixForBLAS(int blasIndex)
{
    int startOffset = blasIndex * kTransformMatrixSize;
    return float3x4(
        asfloat(gTransformMatrixBuffer.Load4(startOffset + 4 * 0)),
        asfloat(gTransformMatrixBuffer.Load4(startOffset + 4 * 4)),
        asfloat(gTransformMatrixBuffer.Load4(startOffset + 4 * 8))
    );
}

float2 getLayerUV(int offset, int index)
{
    return asfloat(gLayerGeometryBuffer.Load2(offset + index * 8));
}
#else // DIRECTX
// TODO: Support material layer data for Vulkan/Metal.
int instanceMaterialBufferOffset(int instanceOffset) { return 0; }
int instanceLayerCount(int instanceOffset) { return 0; }
int instanceLayerMaterialBufferOffset(int instanceOffset, int layerIdx) { return 0; }
int instanceLayerUVBufferOffset(int instanceOffset, int layerIdx) { return 0; }
float2 getLayerUV(int offset, int index) { return 0; }

float3x4 transformMatrixForBLAS(int blasIndex)
{
    return float3x4(1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0);
}
#endif // DIRECTX

float2 computeLayerUV(int instanceOffset, int layerIdx, ShadingData shading)
{
    int uvOffset = instanceLayerUVBufferOffset(instanceOffset, layerIdx);

    float2 uv = 0;
    for (uint i = 0; i < 3; i++)
    {
        uv += shading.barycentrics[i] * getLayerUV(uvOffset, shading.indices[i]);
    }

    return uv;
}

// =================================================================================================
// Common material functions
// =================================================================================================

#if DIRECTX
// Get shader index from material header.
int materialShaderIndex(int mtlOffset)
{
    return gGlobalMaterialConstants.Load(mtlOffset + 0);
}

// Get Nth texture index from material header.
int materialTextureIndex(int mtlOffset, int textureNo)
{
    return gGlobalMaterialConstants.Load(mtlOffset + 4 * (1 + textureNo));
}

// Get Nth sampler index from material header.
int materialSamplerIndex(int mtlOffset, int textureNo)
{
    return gGlobalMaterialConstants.Load(mtlOffset + 4 * (1 + kMaterialMaxTextures + textureNo));
}

// Get Nth texture using index from material header.
Texture2D<float4> materialTexture(int mtlOffset, int textureNo)
{
    int idx = materialTextureIndex(mtlOffset, textureNo);

    return gGlobalMaterialTextures[idx];
}

// Get Nth sampler using index from material header.
SamplerState materialSampler(int mtlOffset, int textureNo)
{
    int idx = materialSamplerIndex(mtlOffset, textureNo);

    return gSamplerArray[idx];
}

// Sample Nth texture using index from material header.
float4 sampleTexture(int mtlOffset, int textureNo, float2 uv, float level)
{
#if VALIDATE_TEXTURE_INDICES
    if (textureNo < 0 || textureNo >= kMaterialMaxTextures)
        return float4(1, 0, 0, 1);
#endif
    Texture2D<float4> txt = materialTexture(mtlOffset, textureNo);
    SamplerState smp = materialSampler(mtlOffset, textureNo);
    return txt.SampleLevel(smp, uv, level);
}
#else // DIRECTX
int materialShaderIndex(int /*mtlOffset*/) { return 0; }
#endif // DIRECTX

#endif // __MATERIALHEADER_H__
