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

// Textures, with hardcoded names. They are stored in the register space after the vertex buffers.
Texture2D<float4> gBaseColorTexture : register(t5, space1);
Texture2D<float4> gSpecularRoughnessTexture : register(t6, space1);
Texture2D<float4> gNormalTexture : register(t7, space1);
Texture2D<float4> gOpacityTexture : register(t8, space1);

// Material properties are stored in this ByteAddressBuffer, and accessed with Material_XXX()
// functions, specific to the material type. They are stored in the register space after the vertex
// and texture buffers.
// NOTE: An untyped ByteAddressBuffer is used instead of a typed constant buffer (e.g.
// ConstantBuffer<MyMaterial123>) because it is not possible in DirectX to have constant buffers with
// different types assigned to the same register.
ByteAddressBuffer gMaterialConstants : register(t9, space1);

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
    float emission;
    float3 emissionColor;
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

// Get property base from byte address buffer
float Material_base(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(0));
}

// Get property base_color from byte address buffer
float3 Material_baseColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(4));
}

// Get property diffuse_roughness from byte address buffer
float Material_diffuseRoughness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(16));
}

// Get property metalness from byte address buffer
float Material_metalness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(20));
}

// Get property specular from byte address buffer
float Material_specular(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(24));
}

// Get property specular_color from byte address buffer
float3 Material_specularColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(32));
}

// Get property specular_roughness from byte address buffer
float Material_specularRoughness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(44));
}

// Get property specular_IOR from byte address buffer
float Material_specularIOR(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(48));
}

// Get property specular_anisotropy from byte address buffer
float Material_specularAnisotropy(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(52));
}

// Get property specular_rotation from byte address buffer
float Material_specularRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(56));
}

// Get property transmission from byte address buffer
float Material_transmission(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(60));
}

// Get property transmission_color from byte address buffer
float3 Material_transmissionColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(64));
}

// Get property subsurface from byte address buffer
float Material_subsurface(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(76));
}

// Get property subsurface_color from byte address buffer
float3 Material_subsurfaceColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(80));
}

// Get property subsurface_radius from byte address buffer
float3 Material_subsurfaceRadius(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(96));
}

// Get property subsurface_scale from byte address buffer
float Material_subsurfaceScale(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(108));
}

// Get property subsurface_anisotropy from byte address buffer
float Material_subsurfaceAnisotropy(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(112));
}

// Get property sheen from byte address buffer
float Material_sheen(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(116));
}

// Get property sheen_color from byte address buffer
float3 Material_sheenColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(128));
}

// Get property sheen_roughness from byte address buffer
float Material_sheenRoughness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(140));
}

// Get property coat from byte address buffer
float Material_coat(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(144));
}

// Get property coat_color from byte address buffer
float3 Material_coatColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(148));
}

// Get property coat_roughness from byte address buffer
float Material_coatRoughness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(160));
}

// Get property coat_anisotropy from byte address buffer
float Material_coatAnisotropy(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(164));
}

// Get property coat_rotation from byte address buffer
float Material_coatRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(168));
}

// Get property coat_IOR from byte address buffer
float Material_coatIOR(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(172));
}

// Get property coat_affect_color from byte address buffer
float Material_coatAffectColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(176));
}

// Get property coat_affect_roughness from byte address buffer
float Material_coatAffectRoughness(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(180));
}

// Get property emission from byte address buffer
float Material_emission(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(184));
}

// Get property emission_color from byte address buffer
float3 Material_emissionColor(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(192));
}

// Get property opacity from byte address buffer
float3 Material_opacity(ByteAddressBuffer buf)
{
    return asfloat(buf.Load3(208));
}

// Get property thin_walled from byte address buffer
int Material_thinWalled(ByteAddressBuffer buf)
{
    return buf.Load(220);
}

// Get property has_base_color_image from byte address buffer
int Material_hasBaseColorTex(ByteAddressBuffer buf)
{
    return buf.Load(224);
}

// Get property base_color_image_offset from byte address buffer
float2 Material_baseColorTexOffset(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(228));
}

// Get property base_color_image_scale from byte address buffer
float2 Material_baseColorTexScale(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(240));
}

// Get property base_color_image_pivot from byte address buffer
float2 Material_baseColorTexPivot(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(248));
}

// Get property base_color_image_rotation from byte address buffer
float Material_baseColorTexRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(256));
}

// Get property has_specular_roughness_image from byte address buffer
int Material_hasSpecularRoughnessTex(ByteAddressBuffer buf)
{
    return buf.Load(260);
}

// Get property specular_roughness_image_offset from byte address buffer
float2 Material_specularRoughnessTexOffset(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(264));
}

// Get property specular_roughness_image_scale from byte address buffer
float2 Material_specularRoughnessTexScale(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(272));
}

// Get property specular_roughness_image_pivot from byte address buffer
float2 Material_specularRoughnessTexPivot(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(280));
}

// Get property specular_roughness_image_rotation from byte address buffer
float Material_specularRoughnessTexRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(288));
}

// Get property has_opacity_image from byte address buffer
int Material_hasOpacityTex(ByteAddressBuffer buf)
{
    return buf.Load(292);
}

// Get property opacity_image_offset from byte address buffer
float2 Material_opacityTexOffset(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(296));
}

// Get property opacity_image_scale from byte address buffer
float2 Material_opacityTexScale(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(304));
}

// Get property opacity_image_pivot from byte address buffer
float2 Material_opacityTexPivot(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(312));
}

// Get property opacity_image_rotation from byte address buffer
float Material_opacityTexRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(320));
}

// Get property has_normal_image from byte address buffer
int Material_hasNormalTex(ByteAddressBuffer buf)
{
    return buf.Load(324);
}

// Get property normal_image_offset from byte address buffer
float2 Material_normalTexOffset(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(328));
}

// Get property normal_image_scale from byte address buffer
float2 Material_normalTexScale(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(336));
}

// Get property normal_image_pivot from byte address buffer
float2 Material_normalTexPivot(ByteAddressBuffer buf)
{
    return asfloat(buf.Load2(344));
}

// Get property normal_image_rotation from byte address buffer
float Material_normalTexRotation(ByteAddressBuffer buf)
{
    return asfloat(buf.Load(352));
}

// Initializes the full set of property values for a material, for the specified shading data.
Material initializeDefaultMaterial(
    ShadingData shading, float3x4 objToWorld, out float3 materialNormal, out bool isGeneratedNormal)
{
    // Copy the constant values to the material from the constant buffer.
    Material material;
    material.base                 = Material_base(gMaterialConstants);
    material.baseColor            = Material_baseColor(gMaterialConstants);
    material.diffuseRoughness     = Material_diffuseRoughness(gMaterialConstants);
    material.metalness            = Material_metalness(gMaterialConstants);
    material.specular             = Material_specular(gMaterialConstants);
    material.specularColor        = Material_specularColor(gMaterialConstants);
    material.specularRoughness    = Material_specularRoughness(gMaterialConstants);
    material.specularIOR          = Material_specularIOR(gMaterialConstants);
    material.specularAnisotropy   = Material_specularAnisotropy(gMaterialConstants);
    material.specularRotation     = Material_specularRotation(gMaterialConstants);
    material.transmission         = Material_transmission(gMaterialConstants);
    material.transmissionColor    = Material_transmissionColor(gMaterialConstants);
    material.subsurface           = Material_subsurface(gMaterialConstants);
    material.subsurfaceColor      = Material_subsurfaceColor(gMaterialConstants);
    material.subsurfaceRadius     = Material_subsurfaceRadius(gMaterialConstants);
    material.subsurfaceScale      = Material_subsurfaceScale(gMaterialConstants);
    material.subsurfaceAnisotropy = Material_subsurfaceAnisotropy(gMaterialConstants);
    material.sheen                = Material_sheen(gMaterialConstants);
    material.sheenColor           = Material_sheenColor(gMaterialConstants);
    material.sheenRoughness       = Material_sheenRoughness(gMaterialConstants);
    material.coat                 = Material_coat(gMaterialConstants);
    material.coatColor            = Material_coatColor(gMaterialConstants);
    material.coatRoughness        = Material_coatRoughness(gMaterialConstants);
    material.coatAnisotropy       = Material_coatAnisotropy(gMaterialConstants);
    material.coatRotation         = Material_coatRotation(gMaterialConstants);
    material.coatIOR              = Material_coatIOR(gMaterialConstants);
    material.coatAffectColor      = Material_coatAffectColor(gMaterialConstants);
    material.coatAffectRoughness  = Material_coatAffectRoughness(gMaterialConstants);
    material.emission             = Material_emission(gMaterialConstants);
    material.emissionColor        = Material_emissionColor(gMaterialConstants);
    material.opacity              = Material_opacity(gMaterialConstants);
    material.thinWalled           = Material_thinWalled(gMaterialConstants);

     // Sample base color from a texture if necessary.
    float4 texCoord = float4(shading.texCoord, 0.0f, 1.0f);
    if (Material_hasBaseColorTex(gMaterialConstants))
    {
        float2 uv =
            applyUVTransform(shading.texCoord, Material_baseColorTexPivot(gMaterialConstants),
                Material_baseColorTexScale(gMaterialConstants),
                Material_baseColorTexRotation(gMaterialConstants),
                Material_baseColorTexOffset(gMaterialConstants));
        material.baseColor = sampleBaseColorTexture(uv, 0.0f).rgb;
    }

    // Sample specular roughness from a texture if necessary.
    if (Material_hasSpecularRoughnessTex(gMaterialConstants))
    {
        float2 uv = applyUVTransform(shading.texCoord,
            Material_specularRoughnessTexPivot(gMaterialConstants),
            Material_specularRoughnessTexScale(gMaterialConstants),
            Material_specularRoughnessTexRotation(gMaterialConstants),
            Material_specularRoughnessTexOffset(gMaterialConstants));

        material.specularRoughness = sampleSpecularRoughnessTexture(uv, 0.0f).r;
    }

    // Sample opacity from a texture if necessary.
    if (Material_hasOpacityTex(gMaterialConstants))
    {
        float2 uv = applyUVTransform(shading.texCoord, Material_opacityTexPivot(gMaterialConstants),
            Material_opacityTexScale(gMaterialConstants),
            Material_opacityTexRotation(gMaterialConstants),
            Material_opacityTexOffset(gMaterialConstants));
        material.opacity = sampleOpacityTexture(uv, 0.0f).rgb;
    }

    // Sample a normal from the normal texture, convert it to an object-space normal, transform to
    // world space, and store it in the output value.
    isGeneratedNormal = false;
    if (Material_hasNormalTex(gMaterialConstants))
    {
        float2 uv = applyUVTransform(shading.texCoord, Material_normalTexPivot(gMaterialConstants),
            Material_normalTexScale(gMaterialConstants),
            Material_normalTexRotation(gMaterialConstants),
            Material_normalTexOffset(gMaterialConstants));
        float3 normalTexel       = sampleNormalTexture(uv, 0.0f).rgb;
        float3 objectSpaceNormal = calculateNormalFromMap(
            normalTexel, TANGENT_SPACE, 1.0, shading.normal, shading.tangent);
        materialNormal = normalize(mul((float3x3)objToWorld, objectSpaceNormal));

        isGeneratedNormal = true;
    }

    // Copy the base color to the (internal) metal color.
    material.metalColor = material.baseColor;

    return material;
}

Material defaultMaterial()
{
    // Copy the constant values to the material from the constant buffer.
    Material material;
    material.base                 = 0.8;
    material.baseColor            = WHITE;
    material.diffuseRoughness     = 0.0;
    material.metalness            = 0.0;
    material.metalColor           = WHITE;
    material.specular             = 1.0;
    material.specularColor        = WHITE;
    material.specularRoughness    = 0.2;
    material.specularIOR          = 1.5;
    material.specularAnisotropy   = 0.0;
    material.specularRotation     = 0.0;
    material.transmission         = 0.0;
    material.transmissionColor    = WHITE;
    material.subsurface           = 0.0;
    material.subsurfaceColor      = WHITE;
    material.subsurfaceRadius     = WHITE;
    material.subsurfaceScale      = 1.0;
    material.subsurfaceAnisotropy = 0.0;
    material.sheen                = 0.0;
    material.sheenColor           = WHITE;
    material.sheenRoughness       = 0.3;
    material.coat                 = 0.0;
    material.coatColor            = WHITE;
    material.coatRoughness        = 0.1;
    material.coatAnisotropy       = 0.0;
    material.coatRotation         = 0.0;
    material.coatIOR              = 1.5;
    material.coatAffectColor      = 0.0;
    material.coatAffectRoughness  = 0.0;
    material.emission             = 0.0;
    material.emissionColor        = WHITE;
    material.opacity              = 1.0;
    material.thinWalled           = 0;

    return material;
}

#endif // __MATERIAL_H__
