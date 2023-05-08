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

#include "SceneBase.h"

BEGIN_AURORA
const string Names::InstanceProperties::kMaterial("material");
const string Names::InstanceProperties::kGeometry("geometry");
const string Names::InstanceProperties::kTransform("transform");
const string Names::InstanceProperties::kObjectID("objectID");
const string Names::InstanceProperties::kVisible("visible");
const string Names::InstanceProperties::kSelectable("selectable");
const string Names::InstanceProperties::kMaterialLayers("layer_materials");
const string Names::InstanceProperties::kGeometryLayers("layer_geometry");

const string Names::EnvironmentProperties::kLightTop("light_top");
const string Names::EnvironmentProperties::kLightBottom("light_bottom");
const string Names::EnvironmentProperties::kLightImage("light_image");
const string Names::EnvironmentProperties::kLightTransform("light_transform");
const string Names::EnvironmentProperties::kBackgroundTop("background_top");
const string Names::EnvironmentProperties::kBackgroundBottom("background_bottom");
const string Names::EnvironmentProperties::kBackgroundImage("background_image");
const string Names::EnvironmentProperties::kBackgroundTransform("background_transform");
const string Names::EnvironmentProperties::kBackgroundUseScreen("background_use_screen");

const string Names::VertexAttributes::kPosition("position");
const string Names::VertexAttributes::kNormal("normal");
const string Names::VertexAttributes::kTexCoord0("tcoord0");
const string Names::VertexAttributes::kTexCoord1("tcoord1");
const string Names::VertexAttributes::kTexCoord2("tcoord2");
const string Names::VertexAttributes::kTexCoord3("tcoord3");
const string Names::VertexAttributes::kTexCoord4("tcoord4");
const string Names::VertexAttributes::kTangent("tangent");
const string Names::VertexAttributes::kIndices("indices");

const string Names::MaterialTypes::kBuiltIn("BuiltIn");
const string Names::MaterialTypes::kMaterialX("MaterialX");
const string Names::MaterialTypes::kMaterialXPath("MaterialXPath");

const string Names::AddressModes::kWrap("Wrap");
const string Names::AddressModes::kMirror("Mirror");
const string Names::AddressModes::kClamp("Clamp");
const string Names::AddressModes::kBorder("Border");
const string Names::AddressModes::kMirrorOnce("MirrorOnce");

const string Names::SamplerProperties::kAddressModeU("AddressModeU");
const string Names::SamplerProperties::kAddressModeV("AddressModeV");

const string Names::LightTypes::kDistantLight("DistantLight");

const std::string Names::LightProperties::kDirection       = "direction";
const std::string Names::LightProperties::kAngularDiameter = "angular_diameter_radians";
const std::string Names::LightProperties::kExposure        = "exposure";
const std::string Names::LightProperties::kIntensity       = "intensity";
const std::string Names::LightProperties::kColor           = "color";

END_AURORA
