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
#include"PathTracingCommon.slang"

// The ray generation shader.
[shader("raygeneration")] void RayGenShader() {
    int maxDepth = gFrameData.traceDepth;

    // Get the dispatch dimensions (screen size), dispatch index (screen coordinates), and sample
    // index.
    uint2 screenSize   = DispatchRaysDimensions().xy;
    uint2 screenCoords = DispatchRaysIndex().xy;
    uint sampleIndex   = gSampleData.sampleIndex;

    // Initialize a random number generator, so that each sample and pixel gets a unique seed.
    Random rng = initRand(sampleIndex, screenSize, screenCoords);

    // Compute a camera ray (origin and direction) for the current screen coordinates. This applies
    // a random offset to support antialiasing and depth of field.
    float3 origin;
    float3 dir;
    computeCameraRay(screenCoords, screenSize, gFrameData.cameraInvView, gFrameData.viewSize,
        gFrameData.isOrthoProjection, gFrameData.focalDistance, gFrameData.lensRadius, rng, origin,
        dir);

    // Trace a radiance ray, and use the returned radiance (color) and alpha.
    // NOTE: Use the background miss shader index for these primary (eye) rays.
    Environment environment = prepareEnvironmentValues();
    RadianceRayPayload rayPayload =
        traceRadianceRay(gScene, environment, origin, dir, 0.0f, 0, maxDepth, true, rng);
    float4 result = float4(rayPayload.color, rayPayload.alpha);
#if DIRECTX
    // Add the contribution of the ground plane, if enabled.
    if (gGroundPlane.enabled)
    {
        // Shade the ground plane.
        Environment environment  = prepareEnvironmentValues();
        float4 groundPlaneResult = shadeGroundPlane(gGroundPlane, gScene, environment, origin, dir,
            rayPayload.depthView, gFrameData.isOpaqueShadowsEnabled, maxDepth, rng);

        // Blend the ground plane result with the radiance and direct lighting.
        // TODO: The latter ensures the ground plane result appears when denoising, but the result
        // itself still needs to be denoised.
        result.rgb        = lerp(result.rgb, groundPlaneResult.rgb, groundPlaneResult.a);
        rayPayload.direct = lerp(rayPayload.direct, groundPlaneResult.rgb, groundPlaneResult.a);
    }
#endif

    // Adjust the radiance of the sample, e.g. to perform corrections.
    adjustRadiance(gFrameData.maxLuminance, gFrameData.isDisplayErrorsEnabled, result.rgb);

    // Store the result in the output texture. The output texture is called "direct" as it contains
    // direct lighting when denoising is enabled, or complete lighting otherwise.
    result.rgb            = gFrameData.isDenoisingEnabled ? rayPayload.direct : result.rgb;
    gDirect[screenCoords] = result;

#if DIRECTX
    // Store the NDC depth value, if enabled. The value is only stored if the first sample was
    // computed (i.e. there is no previously stored value), or it is less than the previously stored
    // value.
    float depthNDC = rayPayload.depthNDC;
    if (gFrameData.isDepthNDCEnabled && (sampleIndex == 0 || depthNDC < gDepthNDC[screenCoords]))
    {
        gDepthNDC[screenCoords] = depthNDC;
    }

    // Prepare and store the data for the denoising AOVs, if enabled.
    // NOTE: Only the data for the last sample is stored, with the expectation that the sample count
    // will be one when these AOVs are enabled.
    if (gFrameData.isDenoisingAOVsEnabled)
    {
        // Remap the normal components from [-1.0, 1.0] to [0.0, 1.0] for unsigned normalized
        // output.
        float3 normalEncoded = (rayPayload.normal + 1.0f) * 0.5f;

        // Prepare the diffuse and glossy radiance and hit distances. The hit distances are
        // normalized relative to the scene size, so they are always in [0.0, 1.0].
        IndirectOutput indirect = rayPayload.indirect;
        float diffuseHitDist    = saturate(indirect.diffuseHitDist / gFrameData.sceneSize);
        float glossyHitDist     = saturate(indirect.glossyHitDist / gFrameData.sceneSize);
        float4 diffusePacked    = float4(indirect.diffuse, diffuseHitDist); // TODO: use NRD packing
        float4 glossyPacked     = float4(indirect.glossy, glossyHitDist);   // TODO: use NRD packing

        // Store the data for the denoising AOVs.
        gDepthView[screenCoords]          = rayPayload.depthView;
        gNormalRoughness[screenCoords]    = float4(normalEncoded, rayPayload.roughness);
        gBaseColorMetalness[screenCoords] = float4(rayPayload.baseColor, rayPayload.metalness);
        gDiffuse[screenCoords]            = diffusePacked;
        gGlossy[screenCoords]             = glossyPacked;
    }
#endif

}
