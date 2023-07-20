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

#include "Properties.h"

BEGIN_AURORA

// A base class for implementations of IEnvironment.
class EnvironmentBase : public IEnvironment, public FixedValues
{
public:
    /*** Lifetime Management ***/

    EnvironmentBase();

    /*** IEnvironment Functions ***/

    FixedValues& values() override { return *this; }

protected:
    struct EnvironmentData
    {
        vec3 lightTop;
        float _padding1;
        vec3 lightBottom;
        float lightTexLuminanceIntegral;
        mat4 lightTransform;
        mat4 lightTransformInv;
        vec3 backgroundTop;
        float _padding3;
        vec3 backgroundBottom;
        float _padding4;
        mat4 backgroundTransform;
        int backgroundUseScreen;
        int hasLightTex;
        int hasBackgroundTex;
    };

    void updateGPUStruct(EnvironmentData& data);
};

END_AURORA