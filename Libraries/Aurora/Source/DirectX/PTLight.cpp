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

#include "PTLight.h"

#include "PTRenderer.h"

BEGIN_AURORA

static PropertySetPtr g_pDistantLightPropertySet;

static PropertySetPtr distantLightPropertySet()
{
    if (g_pDistantLightPropertySet)
    {
        return g_pDistantLightPropertySet;
    }

    // Create properties and defaults for distant lights.
    g_pDistantLightPropertySet = make_shared<PropertySet>();
    g_pDistantLightPropertySet->add(
        Names::LightProperties::kDirection, normalize(vec3(-1.0f, -0.5f, -1.0f)));
    g_pDistantLightPropertySet->add(Names::LightProperties::kColor, vec3(1, 1, 1));
    g_pDistantLightPropertySet->add(Names::LightProperties::kAngularDiameter, 0.1f);
    g_pDistantLightPropertySet->add(Names::LightProperties::kExposure, 0.0f);
    g_pDistantLightPropertySet->add(Names::LightProperties::kIntensity, 1.0f);

    return g_pDistantLightPropertySet;
}

// Return the property set for the provided light type.
static PropertySetPtr propertySet(const string& type)
{
    if (type.compare(Names::LightTypes::kDistantLight) == 0)
        return distantLightPropertySet();

    AU_FAIL("Unknown light type:%s", type.c_str());
    return nullptr;
}

PTLight::PTLight(PTScene* pScene, const string& lightType, int index) :
    FixedValues(propertySet(lightType)), _pScene(pScene), _index(index)
{
}

END_AURORA
