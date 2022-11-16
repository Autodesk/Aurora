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

#ifndef __GLSL_TO_HLSL_H__
#define __GLSL_TO_HLSL_H__

// Map the vector types
#define vec4 float4
#define vec3 float3
#define vec2 float2
#define uvec4 int4
#define uvec3 int3
#define uvec2 int2

// Map mix to lerp.
#define mix lerp
// TODO: other built-ins.

// 'texture' is reserved name, so rename in preprocessor.
#define texture sampleTextureStruct
// Assume float4 texture types.
#define DXTexture Texture2D<float4>

// Map GLSL sampler2D to HLSL struct containing texture and sampler.
struct sampler2D
{
    DXTexture textureObject;
    SamplerState sampler;
};

// Create sampler2D struct, called by HLSL wrapper code.
sampler2D createSampler2D(DXTexture textureObject, SamplerState sampler)
{
    sampler2D txt;
    txt.textureObject = textureObject;
    txt.sampler       = sampler;
    return txt;
}

// Implement GLSL textureSize.
uint2 textureSize(sampler2D txt, int lod)
{
    float2 res       = uint2(1, 1);
    float numLevels = 0;
    txt.textureObject.GetDimensions(lod, res.x, res.y, numLevels);
    return res;
}

// Implement GLSL texture function (renamed via define above).
float4 sampleTextureStruct(sampler2D txt, vec2 uv)
{
    return txt.textureObject.SampleLevel(txt.sampler, uv, 0.0f);
}

// Implement dFdx
// TODO: This is horrible hacky solution needed by mx_compute_sample_size_uv,
// should use different solution that doesn't require this.
float2 dFdx(float2 val)
{
    // Return arbitrary small value.
    return float2(1.0 / 256.0, 1.0 / 256.0);
}

// Implement dFdx
// TODO: This is horrible hacky solution needed by mx_compute_sample_size_uv,
// should use different solution that doesn't require this.
float2 dFdy(float2 val)
{
    // Return arbitrary small value.
    return float2(1.0 / 256.0, 1.0 / 256.0);
}

#endif // __GLSL_TO_HLSL_H__
