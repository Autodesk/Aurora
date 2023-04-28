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

// Material properties used by the built-in Standard Surface material type.
// NOTE: Default values and order come from the Standard surface reference document:
// https://github.com/Autodesk/standard-surface/blob/master/reference/standard_surface.mtlx
UniformBufferDefinition MaterialBase::StandardSurfaceUniforms = {
    UniformBufferPropertyDefinition("base", "base", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("base_color", "baseColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition(
        "diffuse_roughness", "diffuseRoughness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("metalness", "metalness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("specular", "specular", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("specular_color", "specularColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition(
        "specular_roughness", "specularRoughness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("specular_IOR", "specularIOR", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "specular_anisotropy", "specularAnisotropy", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "specular_rotation", "specularRotation", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("transmission", "transmission", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "transmission_color", "transmissionColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition("subsurface", "subsurface", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "subsurface_color", "subsurfaceColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition(
        "subsurface_radius", "subsurfaceRadius", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition(
        "subsurface_scale", "subsurfaceScale", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "subsurface_anisotropy", "subsurfaceAnisotropy", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("sheen", "sheen", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("sheen_color", "sheenColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition(
        "sheen_roughness", "sheenRoughness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("coat", "coat", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("coat_color", "coatColor", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition("coat_roughness", "coatRoughness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "coat_anisotropy", "coatAnisotropy", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("coat_rotation", "coatRotation", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("coat_IOR", "coatIOR", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "coat_affect_color", "coatAffectColor", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "coat_affect_roughness", "coatAffectRoughness", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("opacity", "opacity", PropertyValue::Type::Float3),
    UniformBufferPropertyDefinition("thin_walled", "thinWalled", PropertyValue::Type::Bool),
    UniformBufferPropertyDefinition(
        "has_base_color_image", "hasBaseColorTex", PropertyValue::Type::Bool),
    UniformBufferPropertyDefinition(
        "base_color_image_offset", "baseColorTexOffset", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "base_color_image_scale", "baseColorTexScale", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "base_color_image_pivot", "baseColorTexPivot", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "base_color_image_rotation", "baseColorTexRotation", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "has_specular_roughness_image", "hasSpecularRoughnessTex", PropertyValue::Type::Bool),
    UniformBufferPropertyDefinition("specular_roughness_image_offset", "specularRoughnessTexOffset",
        PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "specular_roughness_image_scale", "specularRoughnessTexScale", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "specular_roughness_image_pivot", "specularRoughnessTexPivot", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition("specular_roughness_image_rotation",
        "specularRoughnessTexRotation", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition(
        "has_opacity_image", "hasOpacityTex", PropertyValue::Type::Bool),
    UniformBufferPropertyDefinition(
        "opacity_image_offset", "opacityTexOffset", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "opacity_image_scale", "opacityTexScale", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "opacity_image_pivot", "opacityTexPivot", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "opacity_image_rotation", "opacityTexRotation", PropertyValue::Type::Float),
    UniformBufferPropertyDefinition("has_normal_image", "hasNormalTex", PropertyValue::Type::Bool),
    UniformBufferPropertyDefinition(
        "normal_image_offset", "normalTexOffset", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "normal_image_scale", "normalTexScale", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "normal_image_pivot", "normalTexPivot", PropertyValue::Type::Float2),
    UniformBufferPropertyDefinition(
        "normal_image_rotation", "normalTexRotation", PropertyValue::Type::Float)
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
    vec3(1.0f, 1.0f, 1.0f), //
    0.1f,                   // coat_roughness coat_anisotropy
    0.0f,                   // coat_anisotropy
    0.0f,                   // coat_rotation
    1.5f,                   // coat_IOR
    0.0f,                   // coat_affect_roughness
    0.0f,                   // coat_affect_color
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
        "has_opacity_image", mtl._values.asImage("opacity_image") ? true : false);
    mtl.uniformBuffer().set("has_normal_image", mtl._values.asImage("normal_image") ? true : false);
}

END_AURORA
