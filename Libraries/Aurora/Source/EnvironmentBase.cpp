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

#include "EnvironmentBase.h"
#include "ImageBase.h"

BEGIN_AURORA

static PropertySetPtr g_pPropertySet;

static PropertySetPtr propertySet()
{
    if (g_pPropertySet)
    {
        return g_pPropertySet;
    }

    g_pPropertySet = make_shared<PropertySet>();

    // Add environment properties and default values to the property set.
    static const vec3 LIGHT_TOP_COLOR         = vec3(1.0f, 1.0f, 1.0f);
    static const vec3 LIGHT_BOTTOM_COLOR      = vec3(0.0f, 0.0f, 0.0f);
    static const vec3 BACKGROUND_TOP_COLOR    = vec3(0.02f, 0.25f, 0.60f);
    static const vec3 BACKGROUND_BOTTOM_COLOR = vec3(0.32f, 0.79f, 1.0f);
    g_pPropertySet->add("light_top", LIGHT_TOP_COLOR);
    g_pPropertySet->add("light_bottom", LIGHT_BOTTOM_COLOR);
    g_pPropertySet->add("light_image", IImagePtr());
    g_pPropertySet->add("light_transform", mat4());
    g_pPropertySet->add("background_top", BACKGROUND_TOP_COLOR);
    g_pPropertySet->add("background_bottom", BACKGROUND_BOTTOM_COLOR);
    g_pPropertySet->add("background_image", IImagePtr());
    g_pPropertySet->add("background_transform", mat4());
    g_pPropertySet->add("background_use_screen", false);

    return g_pPropertySet;
}

EnvironmentBase::EnvironmentBase() : FixedValues(propertySet()) {}

void EnvironmentBase::updateGPUStruct(EnvironmentData& data)
{
    data.lightTop            = _values.asFloat3("light_top");
    data.lightBottom         = _values.asFloat3("light_bottom");
    data.lightTransform      = transpose(_values.asMatrix("light_transform"));
    data.lightTransformInv   = transpose(inverse(_values.asMatrix("light_transform")));
    data.backgroundTop       = _values.asFloat3("background_top");
    data.backgroundBottom    = _values.asFloat3("background_bottom");
    data.backgroundTransform = transpose(_values.asMatrix("background_transform"));
    data.backgroundUseScreen = _values.asBoolean("background_use_screen") ? 1 : 0;
    data.hasBackgroundTex    = _values.asImage("background_image") ? 1 : 0;

    shared_ptr<ImageBase> pLightTex =
        dynamic_pointer_cast<ImageBase>(_values.asImage("light_image"));
    data.hasLightTex = pLightTex ? 1 : 0;
    if (pLightTex)
    {
        data.lightTexLuminanceIntegral = pLightTex->luminanceIntegral();
    }
}

END_AURORA
