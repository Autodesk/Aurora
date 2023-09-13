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
#ifndef __MATERIALHEADER_H__
#define __MATERIALHEADER_H__

#include "GlobalPipelineState.slang"

#define kMaterialMaxTextures 7
#define kMaterialHeaderSize 32

// =================================================================================================
// Common material functions
// =================================================================================================

int materialShaderIndex(int mtlOffset)
{
    return gGlobalMaterialConstants.Load(mtlOffset + 0);
}

int materialTextureIndex(int mtlOffset, int textureNo)
{
    return gGlobalMaterialConstants.Load(mtlOffset + 4 * (1 + textureNo));
}

Texture2D<float4> materialTexture(int mtlOffset, int textureNo)
{
    int idx = materialTextureIndex(mtlOffset, textureNo);

    return gGlobalMaterialTextures[idx];
}

float4 sampleTexture(int mtlOffset, int textureNo, float2 uv, float level, SamplerState sampler)
{
#if VALIDATE_TEXTURE_INDICES
    if (textureNo < 0 || textureNo >= kMaterialMaxTextures)
        return float4(1, 0, 0, 1);
#endif
    Texture2D<float4> txt = materialTexture(mtlOffset, textureNo);
    return txt.SampleLevel(sampler, uv, level);
}


#endif // __MATERIALHEADER_H__
