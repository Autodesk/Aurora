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

// Maxiumum number of textures per-material.
// NOTE: Must match value defined in PTScene.cpp,
#define kMaterialMaxTextures 8

// Total size of fixed-length material header in bytes.
// Material header occurs before each material's properties in the gGlobalMaterialConstants buffer.
// NOTE: Must match size of MaterialHeader in PTScene.cpp.
#define kMaterialHeaderSize 68


// =================================================================================================
// Instance buffer accessor functions
// =================================================================================================

// Get the material offset from instance header.
int instanceMaterialBufferOffset(int instanceOffset)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * 12);
}

// Get the layer count from instance header.
int instanceLayerCount(int instanceOffset)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * 13);
}

// Get the layer material buffer offset for Nth layer.
// Layer data is stored after the header in the instance buffer.
int instanceLayerMaterialBufferOffset(int instanceOffset, int layerIdx)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * (14 + layerIdx * 2));
}

// Get the layer UV buffer offset for Nth layer.
// Layer data is stored after the header in the instance buffer.
// TODO: Not currently implemented.
int instanceLayerUVBufferOffset(int instanceOffset, int layerIdx)
{
    return gGlobalInstanceBuffer.Load(instanceOffset + 4 * (15 + layerIdx * 2));
}

// Get row-major 4x3 world transform matrix for the instance from the buffer.
float3x4 instanceTransformMatrix(int instanceOffset)
{
    return float3x4(
        asfloat(gGlobalInstanceBuffer.Load4(instanceOffset + 4 * 0)),
        asfloat(gGlobalInstanceBuffer.Load4(instanceOffset + 4 * 4)),
        asfloat(gGlobalInstanceBuffer.Load4(instanceOffset + 4 * 8))
    );
}


float2 getLayerUV(int offset, int index)
{
    return asfloat(gLayerGeometryBuffer.Load2(offset + index * 8));
}

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

#endif // __MATERIALHEADER_H__
