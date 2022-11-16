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

#include "HGIGroundPlane.h"

#include "HGIImage.h"
#include "HGIRenderer.h"

BEGIN_AURORA

// The global property set for ground plane data.
static PropertySetPtr gpPropertySet;

// Creates (if needed) and get the property set for ground plane data.
static PropertySetPtr propertySet()
{
    // Return the property set, or create it if it doesn't exist.
    if (gpPropertySet)
    {
        return gpPropertySet;
    }
    gpPropertySet = make_shared<PropertySet>();

    // Add ground plane properties and default values to the property set.
    gpPropertySet->add("enabled", true);
    gpPropertySet->add("position", vec3(0.0f, 0.0f, 0.0f));
    gpPropertySet->add("normal", vec3(0.0f, 1.0f, 0.0f));
    gpPropertySet->add("shadow_opacity", 1.0f);
    gpPropertySet->add("shadow_color", vec3(0.0f, 0.0f, 0.0f));
    gpPropertySet->add("reflection_opacity", 0.5f);
    gpPropertySet->add("reflection_color", vec3(0.5f, 0.5f, 0.5f));
    gpPropertySet->add("reflection_roughness", 0.1f);

    return gpPropertySet;
}

HGIGroundPlane::HGIGroundPlane(HGIRenderer* /* pRenderer */) : FixedValues(propertySet()) {}

END_AURORA