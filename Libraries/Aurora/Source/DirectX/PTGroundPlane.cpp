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

#include "PTGroundPlane.h"

#include "PTRenderer.h"
#include "Properties.h"

BEGIN_AURORA

// Builds basis vectors (tangent and bitangent) from a normal vector.
static void buildBasis(const vec3& normal, vec3& tangent, vec3& bitangent)
{
    bitangent = abs(normal.y) < 0.999f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
    tangent   = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);
}

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

PTGroundPlane::PTGroundPlane(PTRenderer* pRenderer) :
    FixedValues(propertySet()), _pRenderer(pRenderer)
{
}

void PTGroundPlane::update()
{
    // Do nothing if the ground plane is not dirty.
    if (!_bIsDirty)
    {
        return;
    }

    // Update the constant buffer with ground plane data, creating the buffer if needed. The lambda
    // here translates values to the data object for the constant buffer (GPU).
    _pRenderer->updateBuffer<GroundPlaneData>(_pConstantBuffer, [this](GroundPlaneData& data) {
        data.enabled             = _values.asBoolean("enabled") ? 1 : 0;
        data.position            = _values.asFloat3("position");
        data.normal              = _values.asFloat3("normal");
        data.shadowOpacity       = _values.asFloat("shadow_opacity");
        data.shadowColor         = _values.asFloat3("shadow_color");
        data.reflectionOpacity   = _values.asFloat("reflection_opacity");
        data.reflectionColor     = _values.asFloat3("reflection_color");
        data.reflectionRoughness = _values.asFloat("reflection_roughness");

        // Build basis vectors from the plane normal.
        // NOTE: In the future, these may be supplied by the client for specific effects.
        buildBasis(data.normal, data.tangent, data.bitangent);
    });

    _bIsDirty = false;
}

END_AURORA
