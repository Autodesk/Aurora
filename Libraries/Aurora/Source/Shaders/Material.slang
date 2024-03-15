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

#include "Globals.slang"

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
