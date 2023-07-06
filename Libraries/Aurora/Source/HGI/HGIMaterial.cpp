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

#include "HGIMaterial.h"
#include "HGIRenderer.h"

using namespace pxr;

BEGIN_AURORA

// Texture UV transform.
struct TextureTransform
{
    vec2 pivot;
    vec2 scale;
    vec2 offset;
    float rotation;
};

struct MaterialData
{
    float base;
    vec3 baseColor;
    float diffuseRoughness;
    float metalness;
    float specular;
    float _padding1;
    vec3 specularColor;
    float specularRoughness;
    float specularIOR;
    float specularAnisotropy;
    float specularRotation;
    float transmission;
    vec3 transmissionColor;
    float subsurface;
    vec3 subsurfaceColor;
    float _padding2;
    vec3 subsurfaceRadius;
    float subsurfaceScale;
    float subsurfaceAnisotropy;
    float sheen;
    vec2 _padding3;
    vec3 sheenColor;
    float sheenRoughness;
    float coat;
    vec3 coatColor;
    float coatRoughness;
    float coatAnisotropy;
    float coatRotation;
    float coatIOR;
    float coatAffectColor;
    float coatAffectRoughness;
    vec2 _padding4;
    vec3 opacity;
    int thinWalled;
    int hasBaseColorTex;
    vec3 _padding5;
    TextureTransform baseColorTexTransform;
    int hasSpecularRoughnessTex;
    TextureTransform specularRoughnessTexTransform;
    int hasOpacityTex;
    TextureTransform opacityTexTransform;
    int hasNormalTex;
    TextureTransform normalTexTransform;
    int isOpaque;
};

HGIMaterial::HGIMaterial(HGIRenderer* pRenderer, shared_ptr<MaterialDefinition> pDef) :
    MaterialBase(pDef), _pRenderer(pRenderer)
{
    // Create buffer descriptor, passing material as initial data.
    HgiBufferDesc uboDesc;
    uboDesc.debugName = "Material UBO";
    uboDesc.usage     = HgiBufferUsageUniform | HgiBufferUsageRayTracingExtensions |
        HgiBufferUsageShaderDeviceAddress;
    uboDesc.byteSize = sizeof(MaterialData);

    // Create UBO.
    _ubo =
        HgiBufferHandleWrapper::create(_pRenderer->hgi()->CreateBuffer(uboDesc), _pRenderer->hgi());
}

void HGIMaterial::updateGPUStruct(MaterialData& data)
{

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
    data.isOpaque = false;
    // computeIsOpaque();
}

void HGIMaterial::update()
{
    // Build a structure from values map into staging buffer.
    MaterialData* pStaging = getStagingAddress<MaterialData>(_ubo);
    updateGPUStruct(*pStaging);

    // Transfer staging buffer to GPU.
    pxr::HgiBlitCmdsUniquePtr blitCmds = _pRenderer->hgi()->CreateBlitCmds();
    pxr::HgiBufferCpuToGpuOp blitOp;
    blitOp.byteSize              = sizeof(MaterialData);
    blitOp.cpuSourceBuffer       = pStaging;
    blitOp.sourceByteOffset      = 0;
    blitOp.gpuDestinationBuffer  = _ubo->handle();
    blitOp.destinationByteOffset = 0;
    blitCmds->CopyBufferCpuToGpu(blitOp);
    _pRenderer->hgi()->SubmitCmds(blitCmds.get());
}

END_AURORA
