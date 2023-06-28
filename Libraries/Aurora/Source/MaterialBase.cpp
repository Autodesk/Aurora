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
#include "pch.h"

#include "MaterialBase.h"

BEGIN_AURORA

static PropertySetPtr g_pPropertySet;

static PropertySetPtr propertySet()
{
    if (g_pPropertySet)
    {
        return g_pPropertySet;
    }

    g_pPropertySet = make_shared<PropertySet>();

    // Images (textures) and associated transforms.
    // NOTE: Default values must be nullptr, as the property set has a lifetime that could exceed
    // the renderer, and images can't be retained outside their renderer.
    g_pPropertySet->add("base_color_image", IImagePtr());
    g_pPropertySet->add("base_color_image_sampler", ISamplerPtr());
    g_pPropertySet->add("base_color_image_offset", vec2());
    g_pPropertySet->add("base_color_image_scale", vec2(1, 1));
    g_pPropertySet->add("base_color_image_pivot", vec2());
    g_pPropertySet->add("base_color_image_rotation", 0.0f);
    g_pPropertySet->add("specular_roughness_image", IImagePtr());
    g_pPropertySet->add("specular_roughness_image_offset", vec2());
    g_pPropertySet->add("specular_roughness_image_scale", vec2(1, 1));
    g_pPropertySet->add("specular_roughness_image_pivot", vec2());
    g_pPropertySet->add("specular_roughness_image_rotation", 0.0f);
    g_pPropertySet->add("specular_color_image", IImagePtr());
    g_pPropertySet->add("specular_color_image_transform", mat4());
    g_pPropertySet->add("coat_color_image", IImagePtr());
    g_pPropertySet->add("coat_color_image_transform", mat4());
    g_pPropertySet->add("coat_roughness_image", IImagePtr());
    g_pPropertySet->add("coat_roughness_image_transform", mat4());
    g_pPropertySet->add("emission_color_image", IImagePtr());
    g_pPropertySet->add("emission_color_image_offset", vec2());
    g_pPropertySet->add("emission_color_image_scale", vec2(1, 1));
    g_pPropertySet->add("emission_color_image_pivot", vec2());
    g_pPropertySet->add("emission_color_image_rotation", 0.0f);
    g_pPropertySet->add("opacity_image", IImagePtr());
    g_pPropertySet->add("opacity_image_offset", vec2());
    g_pPropertySet->add("opacity_image_scale", vec2(1, 1));
    g_pPropertySet->add("opacity_image_pivot", vec2());
    g_pPropertySet->add("opacity_image_rotation", 0.0f);
    g_pPropertySet->add("opacity_image_sampler", ISamplerPtr());
    g_pPropertySet->add("normal_image", IImagePtr());
    g_pPropertySet->add("normal_image_offset", vec2());
    g_pPropertySet->add("normal_image_scale", vec2(1, 1));
    g_pPropertySet->add("normal_image_pivot", vec2());
    g_pPropertySet->add("normal_image_rotation", 0.0f);
    g_pPropertySet->add("displacement_image", IImagePtr());

    return g_pPropertySet;
}

// A shortcut macro for defining a uniform buffer property definition.
#define PROPERTY_DEF(NAME1, NAME2, TYPE)                                                           \
    UniformBufferPropertyDefinition(NAME1, NAME2, PropertyValue::Type::TYPE)

// Material properties used by the built-in Standard Surface material type.
// NOTE: Default values and order come from the Standard surface reference document:
// https://github.com/Autodesk/standard-surface/blob/master/reference/standard_surface.mtlx
UniformBufferDefinition MaterialBase::StandardSurfaceUniforms = {
    PROPERTY_DEF("base", "base", Float),
    PROPERTY_DEF("base_color", "baseColor", Float3),
    PROPERTY_DEF("diffuse_roughness", "diffuseRoughness", Float),
    PROPERTY_DEF("metalness", "metalness", Float),
    PROPERTY_DEF("specular", "specular", Float),
    PROPERTY_DEF("specular_color", "specularColor", Float3),
    PROPERTY_DEF("specular_roughness", "specularRoughness", Float),
    PROPERTY_DEF("specular_IOR", "specularIOR", Float),
    PROPERTY_DEF("specular_anisotropy", "specularAnisotropy", Float),
    PROPERTY_DEF("specular_rotation", "specularRotation", Float),
    PROPERTY_DEF("transmission", "transmission", Float),
    PROPERTY_DEF("transmission_color", "transmissionColor", Float3),
    PROPERTY_DEF("subsurface", "subsurface", Float),
    PROPERTY_DEF("subsurface_color", "subsurfaceColor", Float3),
    PROPERTY_DEF("subsurface_radius", "subsurfaceRadius", Float3),
    PROPERTY_DEF("subsurface_scale", "subsurfaceScale", Float),
    PROPERTY_DEF("subsurface_anisotropy", "subsurfaceAnisotropy", Float),
    PROPERTY_DEF("sheen", "sheen", Float),
    PROPERTY_DEF("sheen_color", "sheenColor", Float3),
    PROPERTY_DEF("sheen_roughness", "sheenRoughness", Float),
    PROPERTY_DEF("coat", "coat", Float),
    PROPERTY_DEF("coat_color", "coatColor", Float3),
    PROPERTY_DEF("coat_roughness", "coatRoughness", Float),
    PROPERTY_DEF("coat_anisotropy", "coatAnisotropy", Float),
    PROPERTY_DEF("coat_rotation", "coatRotation", Float),
    PROPERTY_DEF("coat_IOR", "coatIOR", Float),
    PROPERTY_DEF("coat_affect_color", "coatAffectColor", Float),
    PROPERTY_DEF("coat_affect_roughness", "coatAffectRoughness", Float),
    PROPERTY_DEF("emission", "emission", Float),
    PROPERTY_DEF("emission_color", "emissionColor", Float3),
    PROPERTY_DEF("opacity", "opacity", Float3),
    PROPERTY_DEF("thin_walled", "thinWalled", Bool),
    PROPERTY_DEF("has_base_color_image", "hasBaseColorTex", Bool),
    PROPERTY_DEF("base_color_image_offset", "baseColorTexOffset", Float2),
    PROPERTY_DEF("base_color_image_scale", "baseColorTexScale", Float2),
    PROPERTY_DEF("base_color_image_pivot", "baseColorTexPivot", Float2),
    PROPERTY_DEF("base_color_image_rotation", "baseColorTexRotation", Float),
    PROPERTY_DEF("has_specular_roughness_image", "hasSpecularRoughnessTex", Bool),
    PROPERTY_DEF("specular_roughness_image_offset", "specularRoughnessTexOffset", Float2),
    PROPERTY_DEF("specular_roughness_image_scale", "specularRoughnessTexScale", Float2),
    PROPERTY_DEF("specular_roughness_image_pivot", "specularRoughnessTexPivot", Float2),
    PROPERTY_DEF("specular_roughness_image_rotation", "specularRoughnessTexRotation", Float),
    PROPERTY_DEF("has_emission_color_image", "hasEmissionColorTex", Bool),
    PROPERTY_DEF("emission_color_image_offset", "emissionColorTexOffset", Float2),
    PROPERTY_DEF("emission_color_image_scale", "emissionColorTexScale", Float2),
    PROPERTY_DEF("emission_color_image_pivot", "emissionColorTexPivot", Float2),
    PROPERTY_DEF("emission_color_image_rotation", "emissionColorTexRotation", Float),
    PROPERTY_DEF("has_opacity_image", "hasOpacityTex", Bool),
    PROPERTY_DEF("opacity_image_offset", "opacityTexOffset", Float2),
    PROPERTY_DEF("opacity_image_scale", "opacityTexScale", Float2),
    PROPERTY_DEF("opacity_image_pivot", "opacityTexPivot", Float2),
    PROPERTY_DEF("opacity_image_rotation", "opacityTexRotation", Float),
    PROPERTY_DEF("has_normal_image", "hasNormalTex", Bool),
    PROPERTY_DEF("normal_image_offset", "normalTexOffset", Float2),
    PROPERTY_DEF("normal_image_scale", "normalTexScale", Float2),
    PROPERTY_DEF("normal_image_pivot", "normalTexPivot", Float2),
    PROPERTY_DEF("normal_image_rotation", "normalTexRotation", Float)
};

// Textures used by the built-in Standard Surface material type.
vector<string> MaterialBase::StandardSurfaceTextures = {
    "base_color_image",
    "specular_roughness_image",
    "opacity_image",
    "normal_image",
};

// Default values for textures used in Standard Surface material type.
vector<TextureDefinition> StandardSurfaceDefaultTextures = {
    { "base_color_image", false },
    { "specular_roughness_image", true },
    { "opacity_image", true },
    { "normal_image", true },
};

// Default values for Standard Surface properties.
// NOTE: These must align with the StandardSurfaceUniforms array above.
vector<PropertyValue> StandardSurfaceDefaultProperties = {
    0.8f,                   // base
    vec3(1.0f, 1.0f, 1.0f), // base_color
    0.0f,                   // diffuse_roughness
    0.0f,                   // metalness
    1.0f,                   // specular
    vec3(1.0f, 1.0f, 1.0f), // specular_color
    0.2f,                   // specular_roughness
    1.5f,                   // specular_IOR
    0.0f,                   // specular_anisotropy
    0.0f,                   // specular_rotation
    0.0f,                   // transmission
    vec3(1.0f, 1.0f, 1.0f), // transmission_color
    0.0f,                   // subsurface
    vec3(1.0f, 1.0f, 1.0f), // subsurface_color
    vec3(1.0f, 1.0f, 1.0f), // subsurface_radius
    1.0f,                   // subsurface_scale
    0.0f,                   // subsurface_anisotropy
    0.0f,                   // sheen
    vec3(1.0f, 1.0f, 1.0f), // sheen_color
    0.3f,                   // sheen_roughness
    0.0f,                   // coat
    vec3(1.0f, 1.0f, 1.0f), // coat_color
    0.1f,                   // coat_roughness
    0.0f,                   // coat_anisotropy
    0.0f,                   // coat_rotation
    1.5f,                   // coat_IOR
    0.0f,                   // coat_affect_roughness
    0.0f,                   // coat_affect_color
    0.0f,                   // emission
    vec3(1.0f, 1.0f, 1.0f), // emission_color
    vec3(1.0f, 1.0f, 1.0f), // opacity
    false,                  // thin_walled
    false,                  // has_base_color_image
    vec2(0.0f, 0.0f),       // base_color_image_offset
    vec2(1.0f, 1.0f),       // base_color_image_scale
    vec2(0.0f, 0.0f),       // base_color_image_pivot
    0.0f,                   // base_color_image_rotation
    false,                  // has_specular_roughness_image
    vec2(0.0f, 0.0f),       // specular_roughness_image_offset
    vec2(1.0f, 1.0f),       // specular_roughness_image_scale
    vec2(0.0f, 0.0f),       // specular_roughness_image_pivot
    0.0f,                   // specular_roughness_image_rotation
    false,                  // has_emission_color_image
    vec2(0.0f, 0.0f),       // emission_color_image_offset
    vec2(1.0f, 1.0f),       // emission_color_image_scale
    vec2(0.0f, 0.0f),       // emission_color_image_pivot
    0.0f,                   // emission_color_image_rotation
    false,                  // has_opacity_image
    vec2(0.0f, 0.0f),       // opacity_image_offset
    vec2(1.0f, 1.0f),       // opacity_image_scale
    vec2(0.0f, 0.0f),       // opacity_image_pivot
    0.0f,                   // opacity_image_rotation
    false,                  // has_normal_image
    vec2(0.0f, 0.0f),       // normal_image_offset
    vec2(1.0f, 1.0f),       // normal_image_scale
    vec2(0.0f, 0.0f),       // normal_image_pivot
    0.0f,                   // normal_image_rotation
};

MaterialDefaultValues MaterialBase::StandardSurfaceDefaults(
    StandardSurfaceUniforms, StandardSurfaceDefaultProperties, StandardSurfaceDefaultTextures);

MaterialBase::MaterialBase(MaterialShaderPtr pShader, MaterialDefinitionPtr pDef) :
    FixedValues(propertySet()),
    _pDef(pDef),
    _pShader(pShader),
    _uniformBuffer(pDef->defaults().propertyDefinitions, pDef->defaults().properties)
{
}

void MaterialBase::updateBuiltInMaterial(MaterialBase& mtl)
{
    // A built-in material is considered opaque if all the opacity components are 1.0, there is no
    // opacity image, and transmission is zero.
    static vec3 kOpaque(1.0f);
    vec3 opacity         = mtl.uniformBuffer().get<vec3>("opacity");
    bool hasOpacityImage = mtl._values.asImage("opacity_image") ? 1 : 0;
    float transmission   = mtl.uniformBuffer().get<float>("transmission");
    mtl.setIsOpaque(opacity == kOpaque && !hasOpacityImage && transmission == 0.0f);

    // Set the image flags used by built-in materials.
    mtl.uniformBuffer().set(
        "has_base_color_image", mtl._values.asImage("base_color_image") ? true : false);
    mtl.uniformBuffer().set("has_specular_roughness_image",
        mtl._values.asImage("specular_roughness_image") ? true : false);
    mtl.uniformBuffer().set(
        "has_emission_color_image", mtl._values.asImage("emission_color_image") ? true : false);
    mtl.uniformBuffer().set(
        "has_opacity_image", mtl._values.asImage("opacity_image") ? true : false);
    mtl.uniformBuffer().set("has_normal_image", mtl._values.asImage("normal_image") ? true : false);
}

END_AURORA
