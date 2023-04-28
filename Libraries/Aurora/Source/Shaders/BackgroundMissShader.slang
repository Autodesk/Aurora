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

// Prefix containing the common code used by all material types.
#include "PathTracingCommon.slang"

// The background miss shader, which evaluates the environment as a background. This is
// typically done from primary, transmission, and transparency rays.
[shader("miss")] void BackgroundMissShader(inout RayPayload rayPayload)
{
    // Initialize the radiance ray payload for a miss.
    rayPayload.radianceRay.clear();

    // Evaluate the environment, as a background.
    Environment environment = prepareEnvironmentValues();
    float3 color            = evaluateEnvironment(environment, WorldRayDirection(), true);

    // Store the environment color.
    // NOTE: The miss result is considered to be part of direct lighting, to allow for simpler logic
    // during accumulation.
    rayPayload.radianceRay.color  = color;
    rayPayload.radianceRay.direct = color;
}
