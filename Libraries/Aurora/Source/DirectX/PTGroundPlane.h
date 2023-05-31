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
#include "MemoryPool.h"

BEGIN_AURORA

class PTRenderer;

// An internal implementation for IGroundPlane.
class PTGroundPlane : public IGroundPlane, public FixedValues
{
public:
    /*** Lifetime Management ***/

    PTGroundPlane(PTRenderer* pRenderer);
    ~PTGroundPlane() {}

    /*** IGroundPlane Functions ***/

    IValues& values() override { return *this; }

    /*** Functions ***/

    ID3D12Resource* buffer() const { return _constantBuffer.pGPUBuffer.Get(); }
    void update();

private:
    /*** Private Types ***/

    struct GroundPlaneData
    {
        int enabled;
        vec3 position;
        vec3 normal;
        float _padding1;
        vec3 tangent;
        float _padding2;
        vec3 bitangent;
        float shadowOpacity;
        vec3 shadowColor;
        float reflectionOpacity;
        vec3 reflectionColor;
        float reflectionRoughness;
    };

    /*** Private Variables ***/

    PTRenderer* _pRenderer;
    TransferBuffer _constantBuffer;
};
MAKE_AURORA_PTR(PTGroundPlane);

END_AURORA
