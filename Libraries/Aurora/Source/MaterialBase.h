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
#pragma once

#include "Properties.h"

BEGIN_AURORA

// Texture UV transform.
struct TextureTransform
{
    vec2 pivot;
    vec2 scale;
    vec2 offset;
    float rotation;
};

// Representation of the shader source for a material type.
struct MaterialTypeSource
{
    MaterialTypeSource(const string& typeName = "", const string& setupSource = "",
        const string& bsdfSource = "") :
        name(typeName), setup(setupSource), bsdf(bsdfSource)
    {
    }

    // Compare the source itself.
    bool compareSource(const MaterialTypeSource& other) const
    {
        return (setup.compare(other.setup) == 0 && bsdf.compare(other.bsdf) == 0);
    }

    // Compare the name.
    bool compare(const MaterialTypeSource& other) const { return (name.compare(other.name) == 0); }

    // Reset the contents of the
    void reset()
    {
        name  = "";
        setup = "";
        bsdf  = "";
    }

    // Is there actually source associated with this material type?
    bool empty() const { return name.empty(); }

    // Unique name.
    string name;

    // Shader source for material setup.
    string setup;

    // Optional shader source for bsdf.
    string bsdf;
};

// A base class for implementations of IMaterial.
class MaterialBase : public IMaterial, public FixedValues
{
public:
    /*** Lifetime Management ***/

    MaterialBase();

    /*** IMaterial Functions ***/

    IValues& values() override { return *this; }

protected:
    // The CPU representation of material constant buffer. The layout of this struct must match the
    // MaterialConstants struct in the shader.
    // NOTE: Members should be kept in the same order as the Standard Surface reference document.
    // https://github.com/Autodesk/standard-surface/blob/master/reference/standard_surface.mtlx
    // Because it is a CPU representation of a GPU constant buffer, it must be padded to conform to
    // DirectX packing conventions:
    // https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
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

    void updateGPUStruct(MaterialData& data);
    bool computeIsOpaque() const;
};

END_AURORA