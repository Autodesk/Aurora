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

    // Constants.
    // NOTE: Default values and order come from the Standard surface reference document:
    // https://github.com/Autodesk/standard-surface/blob/master/reference/standard_surface.mtlx
    g_pPropertySet->add("base", 0.8f);
    g_pPropertySet->add("base_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("diffuse_roughness", 0.0f);
    g_pPropertySet->add("metalness", 0.0f);
    g_pPropertySet->add("specular", 1.0f);
    g_pPropertySet->add("specular_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("specular_roughness", 0.2f);
    g_pPropertySet->add("specular_IOR", 1.5f);
    g_pPropertySet->add("specular_anisotropy", 0.0f);
    g_pPropertySet->add("specular_rotation", 0.0f);
    g_pPropertySet->add("transmission", 0.0f);
    g_pPropertySet->add("transmission_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("transmission_depth", 0.0f);
    g_pPropertySet->add("transmission_scatter", vec3(0.0f, 1.0f, 1.0f));
    g_pPropertySet->add("transmission_scatter_anisotropy", 0.0f);
    g_pPropertySet->add("transmission_dispersion", 0.0f);
    g_pPropertySet->add("transmission_extra_roughness", 0.0f);
    g_pPropertySet->add("subsurface", 0.0f);
    g_pPropertySet->add("subsurface_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("subsurface_radius", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("subsurface_scale", 1.0f);
    g_pPropertySet->add("subsurface_anisotropy", 0.0f);
    g_pPropertySet->add("sheen", 0.0f);
    g_pPropertySet->add("sheen_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("sheen_roughness", 0.3f);
    g_pPropertySet->add("coat", 0.0f);
    g_pPropertySet->add("coat_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("coat_roughness", 0.1f);
    g_pPropertySet->add("coat_anisotropy", 0.0f);
    g_pPropertySet->add("coat_rotation", 0.0f);
    g_pPropertySet->add("coat_IOR", 1.5f);
    g_pPropertySet->add("coat_affect_color", 0.0f);
    g_pPropertySet->add("coat_affect_roughness", 0.0f);
    g_pPropertySet->add("thin_film_thickness", 0.0f);
    g_pPropertySet->add("thin_film_IOR", 1.5f);
    g_pPropertySet->add("emission", 0.0f);
    g_pPropertySet->add("emission_color", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("opacity", vec3(1.0f, 1.0f, 1.0f));
    g_pPropertySet->add("thin_walled", false);

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

void MaterialBase::updateGPUStruct(MaterialData& data)
{
    // Get print offsets into struct for debugging purposes.
#if 0
    int id = 0;
    AU_INFO("Offset %02d  - base:%d", id++, offsetof(MaterialData, base));
    AU_INFO("Offset %02d - baseColor:%d", id++, offsetof(MaterialData, baseColor));
    AU_INFO("Offset %02d  - diffuseRoughness:%d", id++, offsetof(MaterialData, diffuseRoughness));
    AU_INFO("Offset %02d  - metalness:%d", id++, offsetof(MaterialData, metalness));
    AU_INFO("Offset %02d  - specular:%d", id++, offsetof(MaterialData, specular));
    AU_INFO("Offset %02d  - specularColor:%d", id++, offsetof(MaterialData, specularColor));
    AU_INFO("Offset %02d  - specularRoughness:%d", id++, offsetof(MaterialData, specularRoughness));
    AU_INFO("Offset %02d  - specularIOR:%d", id++, offsetof(MaterialData, specularIOR));
    AU_INFO("Offset %02d  - specularAnisotropy:%d", id++, offsetof(MaterialData, specularAnisotropy));
    AU_INFO("Offset %02d - specularRotation:%d", id++, offsetof(MaterialData, specularRotation));
    AU_INFO("Offset %02d - transmission:%d", id++, offsetof(MaterialData, transmission));
    AU_INFO("Offset %02d - transmissionColor:%d", id++, offsetof(MaterialData, transmissionColor));
    AU_INFO("Offset %02d - subsurface:%d", id++, offsetof(MaterialData, subsurface));
    AU_INFO("Offset %02d - subsurfaceColor:%d", id++, offsetof(MaterialData, subsurfaceColor));
    AU_INFO("Offset %02d - subsurfaceRadius:%d", id++, offsetof(MaterialData, subsurfaceRadius));
    AU_INFO("Offset %02d - subsurfaceScale:%d", id++, offsetof(MaterialData, subsurfaceScale));
    AU_INFO("Offset %02d - subsurfaceAnisotropy:%d", id++, offsetof(MaterialData, subsurfaceAnisotropy));
    AU_INFO("Offset %02d - sheen:%d", id++, offsetof(MaterialData, sheen));
    AU_INFO("Offset %02d - sheenColor:%d", id++, offsetof(MaterialData, sheenColor));
    AU_INFO("Offset %02d - sheenRoughness:%d", id++, offsetof(MaterialData, sheenRoughness));
    AU_INFO("Offset %02d - coat:%d", id++, offsetof(MaterialData, coat));
    AU_INFO("Offset %02d - coatColor:%d", id++, offsetof(MaterialData, coatColor));
    AU_INFO("Offset %02d - coatRoughness:%d", id++, offsetof(MaterialData, coatRoughness));
    AU_INFO("Offset %02d - coatAnisotropy:%d", id++, offsetof(MaterialData, coatAnisotropy));
    AU_INFO("Offset %02d - coatRotation:%d", id++, offsetof(MaterialData, coatRotation));
    AU_INFO("Offset %02d - coatIOR:%d", id++, offsetof(MaterialData, coatIOR));
    AU_INFO("Offset %02d - coatAffectColor:%d", id++, offsetof(MaterialData, coatAffectColor));
    AU_INFO(
        "Offset %02d - coatAffectRoughness:%d", id++, offsetof(MaterialData, coatAffectRoughness));
    AU_INFO("Offset %02d - _padding4:%d", id++, offsetof(MaterialData, _padding4));
    AU_INFO("Offset %02d - opacity:%d", id++, offsetof(MaterialData, opacity));
    AU_INFO("Offset %02d - thinWalled:%d", id++, offsetof(MaterialData, thinWalled));
    AU_INFO("Offset %02d - hasBaseColorTex:%d", id++, offsetof(MaterialData, hasBaseColorTex));
    AU_INFO("Offset %02d - baseColorTexTransform:%d", id++, offsetof(MaterialData, baseColorTexTransform));
    AU_INFO("Offset %02d - hasSpecularRoughnessTex:%d", id++, offsetof(MaterialData, hasSpecularRoughnessTex));
    AU_INFO("Offset %02d - specularRoughnessTexTransform:%d", id++, offsetof(MaterialData, specularRoughnessTexTransform));
    AU_INFO("Offset %02d - hasOpacityTex:%d", id++, offsetof(MaterialData, hasOpacityTex));
    AU_INFO("Offset %02d - opacityTexTransform:%d", id++, offsetof(MaterialData, opacityTexTransform));
    AU_INFO("Offset %02d - hasNormalTex:%d", id++, offsetof(MaterialData, hasNormalTex));
    AU_INFO("Offset %02d - normalTexTransform:%d", id++, offsetof(MaterialData, normalTexTransform));
    AU_INFO("Offset %02d - isOpaque:%d", id++, offsetof(MaterialData, isOpaque));
#endif

    // Update the GPU struct from the values map.
    data.base                                 = _values.asFloat("base");
    data.baseColor                            = _values.asFloat3("base_color");
    data.diffuseRoughness                     = _values.asFloat("diffuse_roughness");
    data.metalness                            = _values.asFloat("metalness");
    data.specular                             = _values.asFloat("specular");
    data.specularColor                        = _values.asFloat3("specular_color");
    data.specularRoughness                    = _values.asFloat("specular_roughness");
    data.specularIOR                          = _values.asFloat("specular_IOR");
    data.specularAnisotropy                   = _values.asFloat("specular_anisotropy");
    data.specularRotation                     = _values.asFloat("specular_rotation");
    data.transmission                         = _values.asFloat("transmission");
    data.transmissionColor                    = _values.asFloat3("transmission_color");
    data.subsurface                           = _values.asFloat("subsurface");
    data.subsurfaceColor                      = _values.asFloat3("subsurface_color");
    data.subsurfaceRadius                     = _values.asFloat3("subsurface_radius");
    data.subsurfaceScale                      = _values.asFloat("subsurface_scale");
    data.subsurfaceAnisotropy                 = _values.asFloat("subsurface_anisotropy");
    data.sheen                                = _values.asFloat("sheen");
    data.sheenColor                           = _values.asFloat3("sheen_color");
    data.sheenRoughness                       = _values.asFloat("sheen_roughness");
    data.coat                                 = _values.asFloat("coat");
    data.coatColor                            = _values.asFloat3("coat_color");
    data.coatRoughness                        = _values.asFloat("coat_roughness");
    data.coatAnisotropy                       = _values.asFloat("coat_anisotropy");
    data.coatRotation                         = _values.asFloat("coat_rotation");
    data.coatIOR                              = _values.asFloat("coat_IOR");
    data.coatAffectColor                      = _values.asFloat("coat_affect_color");
    data.coatAffectRoughness                  = _values.asFloat("coat_affect_roughness");
    data.opacity                              = _values.asFloat3("opacity");
    data.thinWalled                           = _values.asBoolean("thin_walled") ? 1 : 0;
    data.hasBaseColorTex                      = _values.asImage("base_color_image") ? 1 : 0;
    data.baseColorTexTransform.offset         = _values.asFloat2("base_color_image_offset");
    data.baseColorTexTransform.scale          = _values.asFloat2("base_color_image_scale");
    data.baseColorTexTransform.pivot          = _values.asFloat2("base_color_image_pivot");
    data.baseColorTexTransform.rotation       = _values.asFloat("base_color_image_rotation");
    data.hasSpecularRoughnessTex              = _values.asImage("specular_roughness_image") ? 1 : 0;
    data.specularRoughnessTexTransform.offset = _values.asFloat2("specular_roughness_image_offset");
    data.specularRoughnessTexTransform.scale  = _values.asFloat2("specular_roughness_image_scale");
    data.specularRoughnessTexTransform.pivot  = _values.asFloat2("specular_roughness_image_pivot");
    data.specularRoughnessTexTransform.rotation =
        _values.asFloat("specular_roughness_image_rotation");
    data.hasOpacityTex                = _values.asImage("opacity_image") ? 1 : 0;
    data.opacityTexTransform.offset   = _values.asFloat2("opacity_image_offset");
    data.opacityTexTransform.scale    = _values.asFloat2("opacity_image_scale");
    data.opacityTexTransform.pivot    = _values.asFloat2("opacity_image_pivot");
    data.opacityTexTransform.rotation = _values.asFloat("opacity_image_rotation");
    data.hasNormalTex                 = _values.asImage("normal_image") ? 1 : 0;
    data.normalTexTransform.offset    = _values.asFloat2("normal_image_offset");
    data.normalTexTransform.scale     = _values.asFloat2("normal_image_scale");
    data.normalTexTransform.pivot     = _values.asFloat2("normal_image_pivot");
    data.normalTexTransform.rotation  = _values.asFloat("normal_image_rotation");

    // Record whether the material is opaque with its current values.
    data.isOpaque = computeIsOpaque();
}

MaterialBase::MaterialBase() : FixedValues(propertySet()) {}

bool MaterialBase::computeIsOpaque() const
{
    // A material is considered opaque if all the opacity components are 1.0, there is no opacity
    // image, and transmission is zero.
    // TODO: This should also consider whether the material type has overridden opacity or
    // transmission, likely with a shader graph attached to that input.
    static vec3 kOpaque(1.0f);
    vec3 opacity         = _values.asFloat3("opacity");
    bool hasOpacityImage = _values.asImage("opacity_image") ? 1 : 0;
    float transmission   = _values.asFloat("transmission");

    return opacity == kOpaque && !hasOpacityImage && transmission == 0.0f;
}

END_AURORA
