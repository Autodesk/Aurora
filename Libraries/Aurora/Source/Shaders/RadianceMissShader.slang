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

#include "PathTracingCommon.slang"

// The radiance miss shader, which evaluates the environment as an environment light.
[shader("miss")]
void RadianceMissShader(inout RayPayload rayPayload)
{
    // Initialize the radiance ray payload for a miss.
    rayPayload.radianceRay.clear();

    // For BSDF importance sampling, evaluate the environment as a light for the final segment of
    // the current path. For any other importance sampling, the environment will be evaluated or
    // sampled for shading at each hit (not here).
    float3 color = BLACK;
    if (IMPORTANCE_SAMPLING_MODE == IMPORTANCE_SAMPLING_BSDF)
    {
        // Evaluate the environment, as a light.
        Environment environment = prepareEnvironmentValues();
        color                   = evaluateEnvironment(environment, WorldRayDirection(), false);
    }

    // Store the environment color.
    // NOTE: The miss result will not be denoised, so it is included in the "extra" shading.
    rayPayload.radianceRay.color = color;
    rayPayload.radianceRay.extra = color;
}