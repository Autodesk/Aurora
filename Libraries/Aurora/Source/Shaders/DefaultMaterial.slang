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
#ifndef __DEFAULTMATERIAL_H__
#define __DEFAULTMATERIAL_H__

#include "Geometry.slang"
#include "Material.slang"
#include "GlobalBufferAccessors.slang"

#if DIRECTX
// DirectX backend uses the autogenerated byte-array accessors.
#include "DefaultMaterialUniformAccessors.slang"
#else

// HGI backend uses the autogenerated uniform buffers.
#include "DefaultMaterialUniformBuffer.slang"
#endif

#if DIRECTX

// Indices of default material textures.  Must match order of StandardSurfaceTextures array in MaterialBase.cpp.
#define BASE_COLOR_TEXTURE_INDEX          0
#define SPECULAR_ROUGHNESS_TEXTURE_INDEX  1
#define OPACITY_TEXTURE_INDEX             2
#define NORMAL_TEXTURE_INDEX              3
#define EMISSION_TEXTURE_INDEX            4

float4 sampleBaseColorTexture(int mtlOffset, float2 uv, float level)
{
    return sampleTexture(mtlOffset, BASE_COLOR_TEXTURE_INDEX, uv, level);
}

float4 sampleSpecularRoughnessTexture(int mtlOffset, float2 uv, float level)
{
    return sampleTexture(mtlOffset, SPECULAR_ROUGHNESS_TEXTURE_INDEX, uv, level);
}

float4 sampleNormalTexture(int mtlOffset, float2 uv, float level)
{
    return sampleTexture(mtlOffset, NORMAL_TEXTURE_INDEX, uv, level);
}

float4 sampleEmissionColorTexture(int mtlOffset, float2 uv, float level)
{
    return sampleTexture(mtlOffset, EMISSION_TEXTURE_INDEX, uv, level);
}

float4 sampleOpacityTexture(int mtlOffset, float2 uv, float level)
{
    return sampleTexture(mtlOffset, OPACITY_TEXTURE_INDEX, uv, level);
}

#else
// Vulkan GLSL versions are forward declared and implemented in raw GLSL suffix file.
MaterialConstants getMaterial();
float4 sampleBaseColorTexture(float2 uv, float level);
float4 sampleSpecularRoughnessTexture(float2 uv, float level);
float4 sampleEmissionColorTexture(float2 uv, float level);
float4 sampleOpacityTexture(float2 uv, float level);
float4 sampleNormalTexture(float2 uv, float level);
#endif

// Normal map spaces definitions.
#define TANGENT_SPACE 0
#define OBJECT_SPACE 1

// Rotate a 2D vector by given number of degrees.
// Based on MaterialX mx_rotate_vector2 functionn.
float2 rotateUV(float2 uv, float amountDegrees)
{
    float rotationRadians = radians(amountDegrees);
    float sa = sin(rotationRadians);
    float ca = cos(rotationRadians);
    return float2(ca * uv.x + sa * uv.y, -sa * uv.x + ca * uv.y);
}

// Rotate a 2D vector by given number of degrees.
// Based on MaterialX NG_place2d_vector2 function.
// NOTE: Applies transform in order Scale-Rotation-Translation
float2 applyUVTransform(float2 uv, float2 pivot, float2 scale, float rotate, float2 offset)
{
    float2 subpivot = uv - pivot;
    float2 scaled = subpivot / scale;
    float2 rotated = rotateUV(scaled, rotate);
    float2 translated = rotated - offset;
    float2 addpivot = translated + pivot;
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
export Material evaluateDefaultMaterial(
    ShadingData shading, int headerOffset, out float3 materialNormal, out bool isGeneratedNormal)
{
    int offset = headerOffset + kMaterialHeaderSize;

    // Copy the constant values to the material from the constant buffer.
    Material material;
    material.base = Material_base(gGlobalMaterialConstants, offset);
    material.baseColor = Material_baseColor(gGlobalMaterialConstants, offset);
    material.diffuseRoughness = Material_diffuseRoughness(gGlobalMaterialConstants, offset);
    material.metalness = Material_metalness(gGlobalMaterialConstants, offset);
    material.specular = Material_specular(gGlobalMaterialConstants, offset);
    material.specularColor = Material_specularColor(gGlobalMaterialConstants, offset);
    material.specularRoughness = Material_specularRoughness(gGlobalMaterialConstants, offset);
    material.specularIOR = Material_specularIOR(gGlobalMaterialConstants, offset);
    material.specularAnisotropy = Material_specularAnisotropy(gGlobalMaterialConstants, offset);
    material.specularRotation = Material_specularRotation(gGlobalMaterialConstants, offset);
    material.transmission = Material_transmission(gGlobalMaterialConstants, offset);
    material.transmissionColor = Material_transmissionColor(gGlobalMaterialConstants, offset);
    material.subsurface = Material_subsurface(gGlobalMaterialConstants, offset);
    material.subsurfaceColor = Material_subsurfaceColor(gGlobalMaterialConstants, offset);
    material.subsurfaceRadius = Material_subsurfaceRadius(gGlobalMaterialConstants, offset);
    material.subsurfaceScale = Material_subsurfaceScale(gGlobalMaterialConstants, offset);
    material.subsurfaceAnisotropy = Material_subsurfaceAnisotropy(gGlobalMaterialConstants, offset);
    material.sheen = Material_sheen(gGlobalMaterialConstants, offset);
    material.sheenColor = Material_sheenColor(gGlobalMaterialConstants, offset);
    material.sheenRoughness = Material_sheenRoughness(gGlobalMaterialConstants, offset);
    material.coat = Material_coat(gGlobalMaterialConstants, offset);
    material.coatColor = Material_coatColor(gGlobalMaterialConstants, offset);
    material.coatRoughness = Material_coatRoughness(gGlobalMaterialConstants, offset);
    material.coatAnisotropy = Material_coatAnisotropy(gGlobalMaterialConstants, offset);
    material.coatRotation = Material_coatRotation(gGlobalMaterialConstants, offset);
    material.coatIOR = Material_coatIOR(gGlobalMaterialConstants, offset);
    material.coatAffectColor = Material_coatAffectColor(gGlobalMaterialConstants, offset);
    material.coatAffectRoughness = Material_coatAffectRoughness(gGlobalMaterialConstants, offset);
    material.emission = Material_emission(gGlobalMaterialConstants, offset);
    material.emissionColor = Material_emissionColor(gGlobalMaterialConstants, offset);
    material.opacity = Material_opacity(gGlobalMaterialConstants, offset);
    material.thinWalled = Material_thinWalled(gGlobalMaterialConstants, offset);

     // Sample base color from a texture if necessary.
    float4 texCoord = float4(shading.texCoord, 0.0f, 1.0f);
    if (Material_hasBaseColorTex(gGlobalMaterialConstants, offset))
    {
        float2 uv =
            applyUVTransform(shading.texCoord, Material_baseColorTexPivot(gGlobalMaterialConstants, offset),
                Material_baseColorTexScale(gGlobalMaterialConstants, offset),
                Material_baseColorTexRotation(gGlobalMaterialConstants, offset),
                Material_baseColorTexOffset(gGlobalMaterialConstants, offset));
        material.baseColor = sampleBaseColorTexture(headerOffset, uv, 0.0f).rgb;
    }

    // Sample specular roughness from a texture if necessary.
    if (Material_hasSpecularRoughnessTex(gGlobalMaterialConstants, offset))
    {
        float2 uv = applyUVTransform(shading.texCoord,
            Material_specularRoughnessTexPivot(gGlobalMaterialConstants, offset),
            Material_specularRoughnessTexScale(gGlobalMaterialConstants, offset),
            Material_specularRoughnessTexRotation(gGlobalMaterialConstants, offset),
            Material_specularRoughnessTexOffset(gGlobalMaterialConstants, offset));

        material.specularRoughness = sampleSpecularRoughnessTexture(headerOffset, uv, 0.0f).r;
    }

    // Sample emission color from a texture if necessary.
    if (Material_hasEmissionColorTex(gGlobalMaterialConstants, offset))
    {
        float2 uv = applyUVTransform(shading.texCoord,
            Material_emissionColorTexPivot(gGlobalMaterialConstants, offset),
            Material_emissionColorTexScale(gGlobalMaterialConstants, offset),
            Material_emissionColorTexRotation(gGlobalMaterialConstants, offset),
            Material_emissionColorTexOffset(gGlobalMaterialConstants, offset));
        material.emissionColor = sampleEmissionColorTexture(headerOffset, uv, 0.0f).rgb;
    }

    // Sample opacity from a texture if necessary.
    if (Material_hasOpacityTex(gGlobalMaterialConstants, offset))
    {
        float2 uv = applyUVTransform(shading.texCoord, Material_opacityTexPivot(gGlobalMaterialConstants, offset),
            Material_opacityTexScale(gGlobalMaterialConstants, offset),
            Material_opacityTexRotation(gGlobalMaterialConstants, offset),
            Material_opacityTexOffset(gGlobalMaterialConstants, offset));
        material.opacity = sampleOpacityTexture(headerOffset, uv, 0.0f).rgb;
    }

    // Sample a normal from the normal texture, convert it to an object-space normal, transform to
    // world space, and store it in the output value.
    isGeneratedNormal = false;
    if (Material_hasNormalTex(gGlobalMaterialConstants, offset))
    {
        float2 uv = applyUVTransform(shading.texCoord, Material_normalTexPivot(gGlobalMaterialConstants, offset),
            Material_normalTexScale(gGlobalMaterialConstants, offset),
            Material_normalTexRotation(gGlobalMaterialConstants, offset),
            Material_normalTexOffset(gGlobalMaterialConstants, offset));
        float3 normalTexel = sampleNormalTexture(headerOffset, uv, 0.0f).rgb;
        float3 objectSpaceNormal = calculateNormalFromMap(
            normalTexel, TANGENT_SPACE, 1.0, shading.normal, shading.tangent);
        materialNormal = normalize(mul((float3x3) shading.objToWorld, objectSpaceNormal));

        isGeneratedNormal = true;
    }

    // Copy the base color to the (internal) metal color.
    material.metalColor = material.baseColor;

    return material;
}

#endif // __MATERIAL_H__
