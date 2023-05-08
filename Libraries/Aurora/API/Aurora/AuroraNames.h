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
#pragma once

namespace Aurora
{

/// Name constants.
/// TODO should use constexpr std::string_view when with support C++17.
namespace Names
{
/// Instance properties
struct InstanceProperties
{
    /// Instance geometry path, modifiying will trigger recreation of instance resource (path)
    static AURORA_API const std::string kGeometry;
    /// Instance material path (path)
    static AURORA_API const std::string kMaterial;
    /// Instance transform matrix (float4x4)
    static AURORA_API const std::string kTransform;
    /// Instance object identifier (int)
    static AURORA_API const std::string kObjectID;
    /// Is instance visible ? (bool)
    static AURORA_API const std::string kVisible;
    /// Is instance selectable ? (bool)
    static AURORA_API const std::string kSelectable;
    /// Array of paths for the materials layers for this instance. (strings)
    /// Array begins at the inner-most layer (closest to base layer) and moves outward.
    static AURORA_API const std::string kMaterialLayers;
    /// Array of paths for the geometry layers for this instance.  Length must less than or equal to
    /// length of layer materials array. (strings)
    /// Array begins at the inner-most layer (closest to base layer) and moves outward.
    static AURORA_API const std::string kGeometryLayers;
};

/// Environment properties.
struct EnvironmentProperties
{
    /// The top color of the gradient used for lighting (float3)
    static AURORA_API const std::string kLightTop;
    /// The bottom color of the gradient used for lighting (float3)
    static AURORA_API const std::string kLightBottom;
    /// The image used for lighting, which takes precedence over the gradient. (path)
    static AURORA_API const std::string kLightImage;
    /// A transformation to apply to the background. (float4x4)
    static AURORA_API const std::string kLightTransform;
    /// The top color of the gradient used for background (float3)
    static AURORA_API const std::string kBackgroundTop;
    /// The top color of the gradient used for background (float3)
    static AURORA_API const std::string kBackgroundBottom;
    /// The image used for background, which takes precedence over the gradient. (path)
    static AURORA_API const std::string kBackgroundImage;
    /// A transformation to apply to the background. (float4x4)
    static AURORA_API const std::string kBackgroundTransform;
    /// Whether to use a simple screen mapping for the background; otherwise a wraparound spherical
    /// mapping is used, default is false (bool)
    static AURORA_API const std::string kBackgroundUseScreen;
};

struct AddressModes
{
    static AURORA_API const std::string kWrap;
    static AURORA_API const std::string kMirror;
    static AURORA_API const std::string kClamp;
    static AURORA_API const std::string kBorder;
    static AURORA_API const std::string kMirrorOnce;
};

/// Sampler properties.
struct SamplerProperties
{
    /// U address mode
    static AURORA_API const std::string kAddressModeU;
    /// V address mode
    static AURORA_API const std::string kAddressModeV;
};

// Vertex attributes.
struct VertexAttributes
{
    static AURORA_API const std::string kPosition;
    static AURORA_API const std::string kNormal;
    static AURORA_API const std::string kTangent;
    static AURORA_API const std::string kTexCoord0;
    static AURORA_API const std::string kTexCoord1;
    static AURORA_API const std::string kTexCoord2;
    static AURORA_API const std::string kTexCoord3;
    static AURORA_API const std::string kTexCoord4;
    static AURORA_API const std::string kIndices;
};

// Material types.
struct MaterialTypes
{
    /// Built-in material type.
    /// Document argument must be one a fixed set of built-in materials for renderer
    /// Use IRenderer::builtInMaterials() to retrieve set of built-in materials supported by
    /// renderer.
    static AURORA_API const std::string kBuiltIn;

    /// MaterialX material type.
    /// Document argument must be MaterialX XML document string.
    static AURORA_API const std::string kMaterialX;

    /// MaterialX path material type.
    /// Document argument must be path to a MaterialX XML file.
    static AURORA_API const std::string kMaterialXPath;
};

/// Types of light.
struct LightTypes
{
    /// Distant (aka directional) light
    static AURORA_API const std::string kDistantLight;
};

/// Light properties (only some will be valid properties for a given light type.)
struct LightProperties
{
    /// Light direction (only valid for LightTypes::kDistantLight.)
    static AURORA_API const std::string kDirection;
    /// The angular diameter of the light in radians (only valid for LightTypes::kDistantLight.)
    static AURORA_API const std::string kAngularDiameter;
    /// Light exposure (defines the power of the light with kIntensity.)
    /// TODO: Implement a more physically accurate photometric model with correct units.
    static AURORA_API const std::string kExposure;
    /// Light intensity (defines the power of the light with kExposure.)
    /// TODO: Implement a more physically accurate photometric model with correct units.
    static AURORA_API const std::string kIntensity;
    /// Light color as RGB.
    static AURORA_API const std::string kColor;
};

} // namespace Names

} // namespace Aurora
