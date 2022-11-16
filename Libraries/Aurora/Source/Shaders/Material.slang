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
#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include "Geometry.slang"

// Texture UV transform represent as scale, offset and rotation.
struct TextureTransform
{
    float2 pivot;
    float2 scale;
    float2 offset;
    float rotation;
};

// Layout of material constants.
// NOTE: This must match the host (CPU) data structure, MaterialData, including the explicit padding
// variables (need due to Vulkan GLSL padding rules)
struct MaterialConstants
{
    float base;
    float3 baseColor;
    float diffuseRoughness;
    float metalness;
    float specular;
    float _padding1;
    float3 specularColor;
    float specularRoughness;
    float specularIOR;
    float specularAnisotropy;
    float specularRotation;
    float transmission;
    float3 transmissionColor;
    float subsurface;
    float _padding2;
    float3 subsurfaceColor;
    float3 subsurfaceRadius;
    float subsurfaceScale;
    float subsurfaceAnisotropy;
    float sheen;
    float2 _padding3;
    float3 sheenColor;
    float sheenRoughness;
    float coat;
    float3 coatColor;
    float coatRoughness;
    float coatAnisotropy;
    float coatRotation;
    float coatIOR;
    float coatAffectColor;
    float coatAffectRoughness;
    float2 _padding4;
    float3 opacity;
    bool thinWalled;
    bool hasBaseColorTex;
    float3 _padding5;
    TextureTransform baseColorTexTransform;
    bool hasSpecularRoughnessTex;
    TextureTransform specularRoughnessTexTransform;
    bool hasOpacityTex;
    TextureTransform opacityTexTransform;
    bool hasNormalTex;
    TextureTransform normalTexTransform;
    bool isOpaque;
};

// The global sampler state, used by default for texture sampling.
[[vk::binding(6)]] SamplerState gDefaultSampler : register(s0);

#if DIRECTX
// Material global variables, for the current acceleration structure instance only.
// NOTE: These must align with the variables declared in the main source file.
ConstantBuffer<MaterialConstants> gMaterialConstants : register(b1, space1);
Texture2D<float4> gBaseColorTexture : register(t4, space1);
Texture2D<float4> gSpecularRoughnessTexture : register(t5, space1);
Texture2D<float4> gNormalTexture : register(t6, space1);
Texture2D<float4> gOpacityTexture : register(t7, space1);

// Samplers for base color and opacity.
// TODO: Add for other textures.
SamplerState gBaseColorSampler : register(s1);
SamplerState gOpacitySampler : register(s2);

float4 sampleBaseColorTexture(float2 uv, float level)
{
    return gBaseColorTexture.SampleLevel(
        gBaseColorSampler, uv, level); // Use the base color sampler, not default sampler.
}
float4 sampleSpecularRoughnessTexture(float2 uv, float level)
{
    return gSpecularRoughnessTexture.SampleLevel(
        gDefaultSampler, uv, level); // Use the default sampler.
}
float4 sampleOpacityTexture(float2 uv, float level)
{
    return gOpacityTexture.SampleLevel(
        gOpacitySampler, uv, level); // Use the opacity sampler, not default sampler.
}
float4 sampleNormalTexture(float2 uv, float level)
{
    return gNormalTexture.SampleLevel(gDefaultSampler, uv, level); // Use the default sampler.
}

#else
// Vulkan GLSL versions are forward declared and implemented in raw GLSL suffix file.
MaterialConstants getMaterial();
float4 sampleBaseColorTexture(float2 uv, float level);
float4 sampleSpecularRoughnessTexture(float2 uv, float level);
float4 sampleOpacityTexture(float2 uv, float level);
float4 sampleNormalTexture(float2 uv, float level);
#endif

// Material values used during material evaluation.
struct Material
{
    float base;
    float3 baseColor;
    float diffuseRoughness;
    float metalness;
    float3 metalColor;
    float specular;
    float3 specularColor;

    float specularRoughness;
    float specularIOR;
    float specularAnisotropy;
    float specularRotation;
    float transmission;
    float3 transmissionColor;
    float subsurface;
    float3 subsurfaceColor;
    float3 subsurfaceRadius;
    float subsurfaceScale;
    float subsurfaceAnisotropy;
    float sheen;
    float3 sheenColor;
    float sheenRoughness;
    float coat;
    float3 coatColor;
    float coatRoughness;
    float coatAnisotropy;
    float coatRotation;
    float coatIOR;
    float coatAffectColor;
    float coatAffectRoughness;
    float3 opacity;
    bool thinWalled;
    bool isOpaque;
};

// Normal map spaces definitions.
#define TANGENT_SPACE 0
#define OBJECT_SPACE 1

// Rotate a 2D vector by given number of degrees.
// Based on MaterialX mx_rotate_vector2 functionn.
float2 rotateUV(float2 uv, float amountDegrees)
{
    float rotationRadians = radians(amountDegrees);
    float sa              = sin(rotationRadians);
    float ca              = cos(rotationRadians);
    return float2(ca * uv.x + sa * uv.y, -sa * uv.x + ca * uv.y);
}

// Rotate a 2D vector by given number of degrees.
// Based on MaterialX NG_place2d_vector2 function.
float2 applyUVTransform(float2 uv, float2 pivot, float2 scale, float rotate, float2 offset)
{
    float2 subpivot   = uv - pivot;
    float2 scaled     = subpivot / scale;
    float2 rotated    = rotateUV(scaled, rotate);
    float2 translated = rotated - offset;
    float2 addpivot   = translated + pivot;
    return addpivot;
}

// Calculate a per-pixel normal from a normal map texel value.
// NOTE: This is based on the MaterialX function mx_normalmap().
float3 calculateNormalFromMap(float3 texelValue, int space, float scale, float3 N, float3 T)
{
    // Remap texel components from [0.0, 1.0] to [-1.0, 1.0].
    float3 v = texelValue * 2.0 - 1.0;

    // If the texel normal is in tangent space, transform it to the coordinate system defined by N
    // and T.
    if (space == TANGENT_SPACE)
    {
        float3 B = normalize(cross(N, T));
        return normalize(T * v.x * scale + B * v.y * scale + N * v.z);
    }

    // Otherwise the texel normal is in object space, and is simply normalized.
    else
    {
        return normalize(v);
    }
}

// Initializes the full set of property values for a material, for the specified shading data.
Material initializeDefaultMaterial(
    ShadingData shading, float3x4 objToWorld, out float3 materialNormal, out bool isGeneratedNormal)
{
#if DIRECTX
    MaterialConstants materialConstants = gMaterialConstants;
#else
    MaterialConstants materialConstants = getMaterial();
#endif

    // Copy the constant values to the material from the constant buffer.
    Material material;
    material.base                 = materialConstants.base;
    material.baseColor            = materialConstants.baseColor;
    material.diffuseRoughness     = materialConstants.diffuseRoughness;
    material.metalness            = materialConstants.metalness;
    material.specular             = materialConstants.specular;
    material.specularColor        = materialConstants.specularColor;
    material.specularRoughness    = materialConstants.specularRoughness;
    material.specularIOR          = materialConstants.specularIOR;
    material.specularAnisotropy   = materialConstants.specularAnisotropy;
    material.specularRotation     = materialConstants.specularRotation;
    material.transmission         = materialConstants.transmission;
    material.transmissionColor    = materialConstants.transmissionColor;
    material.subsurface           = materialConstants.subsurface;
    material.subsurfaceColor      = materialConstants.subsurfaceColor;
    material.subsurfaceRadius     = materialConstants.subsurfaceRadius;
    material.subsurfaceScale      = materialConstants.subsurfaceScale;
    material.subsurfaceAnisotropy = materialConstants.subsurfaceAnisotropy;
    material.sheen                = materialConstants.sheen;
    material.sheenColor           = materialConstants.sheenColor;
    material.sheenRoughness       = materialConstants.sheenRoughness;
    material.coat                 = materialConstants.coat;
    material.coatColor            = materialConstants.coatColor;
    material.coatRoughness        = materialConstants.coatRoughness;
    material.coatAnisotropy       = materialConstants.coatAnisotropy;
    material.coatRotation         = materialConstants.coatRotation;
    material.coatIOR              = materialConstants.coatIOR;
    material.coatAffectColor      = materialConstants.coatAffectColor;
    material.coatAffectRoughness  = materialConstants.coatAffectRoughness;
    material.opacity              = materialConstants.opacity;
    material.thinWalled           = materialConstants.thinWalled;
    material.isOpaque             = materialConstants.isOpaque;

    // Sample base color from a texture if necessary.
    float4 texCoord = float4(shading.texCoord, 0.0f, 1.0f);
    if (materialConstants.hasBaseColorTex)
    {
        float2 uv =
            applyUVTransform(shading.texCoord, materialConstants.baseColorTexTransform.pivot,
                materialConstants.baseColorTexTransform.scale,
                materialConstants.baseColorTexTransform.rotation,
                materialConstants.baseColorTexTransform.offset);
        material.baseColor = sampleBaseColorTexture(uv, 0.0f).rgb;
    }

    // Sample specular roughness from a texture if necessary.
    if (materialConstants.hasSpecularRoughnessTex)
    {
        float2 uv                  = applyUVTransform(shading.texCoord,
            materialConstants.specularRoughnessTexTransform.pivot,
            materialConstants.specularRoughnessTexTransform.scale,
            materialConstants.specularRoughnessTexTransform.rotation,
            materialConstants.specularRoughnessTexTransform.offset);
        material.specularRoughness = sampleSpecularRoughnessTexture(uv, 0.0f).r;
    }

    // Sample opacity from a texture if necessary.
    if (materialConstants.hasOpacityTex)
    {
        float2 uv = applyUVTransform(shading.texCoord, materialConstants.opacityTexTransform.pivot,
            materialConstants.opacityTexTransform.scale,
            materialConstants.opacityTexTransform.rotation,
            materialConstants.opacityTexTransform.offset);
        material.opacity = sampleOpacityTexture(uv, 0.0f).rgb;
    }

    // Set generated normal flag to false, as normal not modified by material.
    isGeneratedNormal = false;

    // Sample a normal from the normal texture, convert it to an object-space normal, transform to
    // world space, and store it in the output value.
    if (materialConstants.hasNormalTex)
    {
        float2 uv = applyUVTransform(shading.texCoord, materialConstants.normalTexTransform.pivot,
            materialConstants.normalTexTransform.scale,
            materialConstants.normalTexTransform.rotation,
            materialConstants.normalTexTransform.offset);
        float3 normalTexel       = sampleNormalTexture(uv, 0.0f).rgb;
        float3 objectSpaceNormal = calculateNormalFromMap(
            normalTexel, TANGENT_SPACE, 1.0, shading.normal, shading.tangent);
        materialNormal = normalize(mul((float3x3)objToWorld, objectSpaceNormal));

        // Set generated normal flag to true, as normal modified by normal map.
        isGeneratedNormal = true;
    }

    // Copy the base color to the (internal) metal color.
    material.metalColor = material.baseColor;

    return material;
}

#endif // __MATERIAL_H__
