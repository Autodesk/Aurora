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
#include "Options.slang"
#include "PathTracingCommon.slang"
#include "ShadeFunctions.slang"
#include "DefaultMaterial.slang"
#include "GlobalBufferAccessors.slang"
#include "InstancePipelineState.slang"
#include "EvaluateMaterial.slang"

// =================================================================================================
// Ray Gen Shader
// =================================================================================================
// Must match root signature defined in PTShaderLibrary::initRootSignatures and the descriptors 
// uploaded to the heap in PTRenderer::updateOutputResources and PTRenderer::updateDenoisingResources.
[[vk::binding(1)]] RWTexture2D<float4> gResult : register(u0);
RWTexture2D<float> gDepthNDC : register(u1);
RWTexture2D<float> gDepthView : register(u2);
RWTexture2D<float4> gNormalRoughness : register(u3);
RWTexture2D<float4> gBaseColorMetalness : register(u4);
RWTexture2D<float4> gDiffuse : register(u5);
RWTexture2D<float4> gGlossy : register(u6);

// Results of the path tracing sample.
struct Output {
    // Depth in Normalized Device Coordinates, for entire path.
    float depthNDC;
    // Normal of first collision in path.
    float3 normal;
    // Material metalness of first collision in path.
    float metalness;
    // Material roughness of first collision in path.
    float roughness;
    // Material base color of first collision in path.
    float3 baseColor;
    // Depth in Normalized Device Coordinates, for entire path.
    float depthView;
    // Direct lighting contribution for path.
    float3 direct;
    // Indirect lighting contribution for path (split into diffuse and glossy.)
    IndirectOutput indirect;
    // Alpha for path (0.0 only if all ray segments were transparent)
    float alpha;

    [mutating]
    void clear() {
        indirect.clear();
        depthNDC = 1.0f;
        normal = -1.0f;
        metalness =  0.1f;
        roughness = 0.1f;
        baseColor = BLACK;
        depthView = INFINITY;
        direct = 0.0f;
        alpha   = 0.0;
    }
}

// The ray generation shader, iteratively shades the entire ray path.
[shader("raygeneration")] void RayGenShader() {
    // Get the max trace depth from frame settings.
    int maxTraceDepth                = gFrameData.traceDepth;

    // Is opaque shadow hits only enabled in frame settings.
    bool onlyOpaqueShadowHits = gFrameData.isForceOpaqueShadowsEnabled;

    // Get the dispatch dimensions (screen size), dispatch index (screen coordinates), and sample
    // index.
    uint2 screenSize   = DispatchRaysDimensions().xy;
    uint2 screenCoords = DispatchRaysIndex().xy;
    uint sampleIndex   = gSampleData.sampleIndex;

    // Initialize a random number generator, so that each sample and pixel gets a unique seed.
    Random rng = initRand(sampleIndex, screenSize, screenCoords);

    // Compute a camera ray (origin and direction) for the current screen coordinates. This applies
    // a random offset to support antialiasing and depth of field.
    float3 rayOrigin;
    float3 rayDirection;
    int rayType        = CAMERA_RAY;
    computeCameraRay(screenCoords, screenSize, gFrameData.cameraInvView, gFrameData.viewSize,
        gFrameData.isOrthoProjection, gFrameData.focalDistance, gFrameData.lensRadius, rng, rayOrigin,
        rayDirection);

    // The results of the tracing the entire ray path through the scene.
    Output output;
    output.clear();

    // The ray t (distance along path.)
    float t = 0;

    // Prepare the environment data structure.
    Environment environment = prepareEnvironmentValues();

    // The ray contribution is how much the current ray segment contributes to the overall path. 
    // It starts at 1.0 and then is scaled downwards at each subsequent segment.
    float3 rayContribution = 1.0f;

    // Is the current ray segment a primary ray?
    bool isPrimaryRay   = true;

    // Has the path been terminated?
    bool pathEnded = false;
    InstanceRayPayload rayPayload;

    // Iterate through every segment in the ray path, until maximum depth is reached or path is ended.
    for (int rayDepth = 0; rayDepth <= maxTraceDepth && !pathEnded; rayDepth++)
    {
        // The minimum T for this ray segment is zero for the initial camera ray otherwise use
        // a small epsilon to avoid self-intersection.
        float minT = rayDepth == 0 ? 0.0f : M_RAY_TMIN;

        // If this ray collides with nothing, should the background or lighting environment be shaded?
        bool shadeBackgroundOnMiss = isPrimaryRay || rayType == TRANSMISSION_LOBE || rayType == GROUND_PLANE_REFLECTION_RAY;

        // Direct light contribution for this ray segment.
        float3 direct = 0;

        // Indirect light contribution for this ray segment.
        IndirectOutput indirect;
        indirect.clear();

        // How much the contribution will be scaled for the subsequent ray segments (based on sampling PDF of this surface's material.)
        float3 nextRayContribution = 1.0;

        // The ray type of the next ray (based on sampling lobe produced from sampling this surface's material.)
        int nextRayType;

        // Have we exceeded ray depth for this path?
        if (rayDepth == maxTraceDepth)
        {
            // If we have reached the end of this path, evaluate the environment and terminate the path.
            direct = evaluateEnvironment(environment, rayDirection, shadeBackgroundOnMiss);
            pathEnded = true;
        }
        else
        {
            bool absorbedByGroundPlane = false;

            // Trace an instance ray that will return the instance shading result from the closest hit, if any.
            traceInstanceRay(gScene, rayOrigin, rayDirection, minT, rayPayload);

            // Add the contribution of the ground plane, if enabled.
            float3 groundPlanePosition = 0;
            float groundPlaneDistance = INFINITY;
            if (isPrimaryRay && gGroundPlane.enabled && intersectGroundPlane(gGroundPlane, rayOrigin, rayDirection, groundPlanePosition, groundPlaneDistance) && groundPlaneDistance < rayPayload.distance)
            {
                float3 groundPlaneRayDirection;
                float3 groundPlaneRayOrigin;
                float3 groundPlaneRayColor;
          
                // Shade the ground plane.
                int groundPlaneRayType = setupGroundPlaneRay(groundPlanePosition, gGroundPlane, gScene, environment, rayOrigin, rayDirection,
                onlyOpaqueShadowHits, maxTraceDepth, rng,
                groundPlaneRayDirection, groundPlaneRayOrigin, groundPlaneRayColor);

                if (groundPlaneRayType == GROUND_PLANE_REFLECTION_RAY)
                {
                    // The ground plane reflection path was selected, continue the path as a reflection ray.
                    nextRayType = GROUND_PLANE_REFLECTION_RAY;
                    
                    // Setup the next ray direction, origin and contribution from ground plane.
                    nextRayContribution = groundPlaneRayColor;
                    rayDirection = groundPlaneRayDirection;
                    rayOrigin = groundPlaneRayOrigin;
 
                    // The ray was absorbed.
                    absorbedByGroundPlane = true;
                    
                    // This ray is no longer a primary ray.
                    isPrimaryRay = false;
                }
                else
                {
                    // If the ray was not absorbed scale the contribution based on ground plane color.
                    rayContribution *= groundPlaneRayColor;
                }
            }
                
            if (!absorbedByGroundPlane)
            {
                // Did the ray segment hit any geometry?
                if (rayPayload.hit())
                {
         
                    // Increment t by the distance of the ray hit.
                    t += rayPayload.distance;
                    
                    // View direction is inverse of ray direction.
                    float3 V = -rayDirection;

                    // Shading data and normal are taken from ray hit results.
                    ShadingData shading = shadingDataFromInstanceResult(rayPayload);

                    // Were any layers hit?  If this remains false, ray was not absorbed by any of the layer, including base layer..
                    bool hitLayer = false;

                    // Get the UV for the base layer (as the UVs in the shading struct will be over-written in layer loop.)
                    float2 baseLayerUV = shading.texCoord;

// Only include code for layer loop if ENABLE_LAYERS is set to 1 (otherwise just set layerIndex to zero.)
#if ENABLE_LAYERS
                    // Get layer count from the instance header.
                    int layerCount = instanceLayerCount(rayPayload.instanceBufferOffset);
                        
                    // Iterate through all the layers in reverse order, ending with base layer.
                    for(int li = layerCount; li>=0; li--) {
#else
                    int li = 0;
                    {
#endif


                        // Compute material buffer offset, either for layer material or base layer.
                        int materialBufferOffset;
                        if (li == 0)
                        {
                            materialBufferOffset = instanceMaterialBufferOffset(rayPayload.instanceBufferOffset);
                            shading.texCoord = baseLayerUV;
                        }
                        else
                        {
                            int layerIndex = li-1;
                            materialBufferOffset = instanceLayerMaterialBufferOffset(rayPayload.instanceBufferOffset, layerIndex);  
                            shading.texCoord = computeLayerUV(rayPayload.instanceBufferOffset, layerIndex, shading);
                        }

                        // Retrieve the shader index for the layer material.
                        // This is stored in the material header in the global material byte buffer,
                        // at a position indicated by instance's gMaterialBufferOffset.
                        int shaderIndex = materialShaderIndex(materialBufferOffset);

                        // Was a normal generated by the material?
                        bool isGeneratedNormal = false;       

                        // Material normal (overriden by material setup, if material has normal map.)
                        float3 materialNormal = shading.normal;

                        // Initialize the material using the shader index with the externally defined evaluate function.
                        // This function will be generated at runtime and linked with the rest of the code.
                        Material material = evaluateMaterialForShader(shaderIndex, shading,
                                     materialBufferOffset, materialNormal, isGeneratedNormal);
                   
                        // Is this a primary ray?
                        if (isPrimaryRay)
                        {
                            // Set the output normal from the first hit.
                            output.normal = materialNormal;

                            // Clamp the output roughness for the ray path to a minimum, because the denoiser
                            // handles materials with low (near-zero) roughness poorly, leading to a noisy result. This addresses
                            // two separate but related issues: low-roughness metallic materials reflecting
                            // noisy surroundings, and low- roughness dielectric materials where the glossy
                            // lobe is sparsely sampled. NOTE: The roughness in the ray payload is used for
                            // denoising only. The material specular roughness (used in shading) is not
                            // modified here.
                            static const float kMinRoughness = 0.05f;
                            float clampedRoughness = max(material.specularRoughness, kMinRoughness);

                            // Set the output materal properties for the first hit.
                            output.metalness = material.metalness;
                            output.roughness = clampedRoughness;
                            output.baseColor = material.baseColor;

                            // Set the output view-space depth from ray t.
                            output.depthView = t;

                            // Set the output Normalized Device Coordinate (NDC) depth from hit position.
                            float4 positionClip =
                                mul(gFrameData.cameraViewProj, float4(shading.geomPosition, 1.0f));
                            output.depthNDC = (positionClip.z / positionClip.w + 1.0f) / 2.0f;
                        }

                        // Compute transparency from inverse of opacity.
                        float3 transparency = 1.0f - material.opacity;

                        // Randomly choose if next ray segment hit any opaque geometry for this layer based on the transparency of the surface.
                        float P = luminance(transparency);
                        hitLayer = random2D(rng).x >= P;
                        if (hitLayer)
                        {
                            // This ray struck opaque geometry, shade the collision as the next ray segment in the path.

                            // If a new normal has been generated, transform it to world space and build a
                            // corresponding basis from it.
                            if (isGeneratedNormal)
                            {
                                shading.normal = materialNormal;
                                buildBasis(shading.normal, shading.tangent, shading.bitangent);
                            }

                            // Add the emissive light for this collision to the direct light for this ray segement.
                            direct += shadeEmission(material);

                            // Shade with a randomly selected global distant light. Skip this if there are
                            // no distant lights.
                            if (gFrameData.lights.distantLightCount > 0)
                            {
                                // Choose a random distant light for this sample, by computing an index in
                                // range the 0 to (distantLightCount-1).
                                int lightIdx =
                                    int(random2D(rng).x * float(gFrameData.lights.distantLightCount));

                                // Skip if the light has zero intensity.
                                if (gFrameData.lights.distantLights[lightIdx].colorAndIntensity.a > 0.0f)
                                {
                                    // Shade the light, add the result to the direct lighting for this segment.
                                    direct += shadeDirectionalLight(
                                        gFrameData.lights.distantLights[lightIdx].direction,
                                        gFrameData.lights.distantLights[lightIdx].colorAndIntensity,
                                        gFrameData.lights.distantLights[lightIdx].cosRadius, gScene,
                                        material, shading, V, onlyOpaqueShadowHits, rayDepth, maxTraceDepth, rng);
                                }
                            }

                            // Shade with the environment light with multiple importance sampling.
                            // NOTE: For denoising purposes, "indirect" lighting includes the environment light.
                            shadeEnvironmentLightMIS(environment, gScene, material, shading, V,
                                onlyOpaqueShadowHits, rayDepth, maxTraceDepth, rng, indirect);

                            // Scale the ray contribution (for this ray segment and subsequent ones) by the inverse of 
                            // the transparency, and normalized by the probability of this segment being considered opaque.
                            rayContribution *= material.opacity / (1.0f - P);

                            // If the output alpha is zero and this was a transmission lobe ray, pass along an
                            // alpha of zero. If all the path segments beyond this hit have transparent or transmissive
                            // the final result will be zero, otherwise the alpha is will be one (opaque).
                            output.alpha = (output.alpha == 0.0f && rayType == TRANSMISSION_LOBE) ? 0.0f : 1.0f;

                            // Sample the PDF for the current surface material, and compute the next ray direction and contribution.
                            nextRayType = setupIndirectLightRay(material, shading, V, rayDepth, maxTraceDepth, rng,
                                rayDirection, rayOrigin, nextRayContribution);

                            // If next ray not needed (as contribution is too small) terminate the path.
                            if (nextRayType == NO_SECONDARY_RAY)
                                pathEnded = true;

                            // This is no longer a primary ray.
                            isPrimaryRay = false;

                            // Break out of loop when ray is absorbed by a layer (if we are in a loop.)
#if ENABLE_LAYERS
                            break;
#endif
                        }
                        // Scale the ray contribution (for this ray segment and subsequent ones) by transparency, and normalized
                        // by the probability of this layer being considered transparent.
                        else  rayContribution *= transparency / P;
                    }

                    // Handle case were no layers were hit, including base layer.
                    if(!hitLayer)
                    {
                        // No opaque geometry was hit in any layer, continue the ray path as if no collision occurred.
                        rayDirection = -V;
                        rayOrigin = shading.geomPosition;

                        // All other ray variables remain the same.

                        // Continue to the next path segment without applying contribution (as if this
                        // hit didn't happen.)
                        continue;
                    }
                }
                else
                {
                    // No geometry was hit, shade the ray using the background color.
                    // If needed otherwise do nothing, as environment lighting calculated during MIS shading.
                    if (shadeBackgroundOnMiss)
                        direct = evaluateEnvironment(environment, rayDirection, true);

                    // Terminate the path.
                    pathEnded = true;
                }
            }
        }
    
        // Scale all the lighting componenents for the current ray segment, indirect and direct, by the contribution.
        direct *= rayContribution;
        indirect.diffuse *= rayContribution;
        indirect.glossy *= rayContribution;

        // How the result is accumulated depends on whether this ray is a primary ray.
        if (isPrimaryRay)
        {
            // If it is a primary ray, accumulate direct and indirect components in to equivalent component in the output data.
            output.direct   = direct;
            output.indirect = indirect;
        }
        else
        {
            // If this secondary ray compute the total radiance from all the lighting components.
            float3 radiance = direct + indirect.diffuse + indirect.glossy;

            // Accumulate the total radiance into diffuse or glossy component of output indirect 
            // component, based on lobe this ray segment originated from.
            if (rayType == DIFFUSE_LOBE)
            {
                output.indirect.diffuse += radiance;
                output.indirect.diffuseHitDist = t;
            }
            else
            {
                output.indirect.glossy += radiance;
                output.indirect.glossyHitDist = t;
            }
        }

        // Scale the contribution to be used in subsequent segments.
        rayContribution *= nextRayContribution;

        // Set the ray type for the next segment.
        rayType = nextRayType;
    }

    float3 finalRadiance = output.direct + output.indirect.diffuse + output.indirect.glossy;
    float4 result = float4(finalRadiance, output.alpha);

    // Adjust the radiance of the sample, e.g. to perform corrections.
    adjustRadiance(gFrameData.maxLuminance, gFrameData.isDisplayErrorsEnabled, result.rgb);

    // Store the result in the "result" output texture. If denoising is enabled, only the "direct"
    // (non-denoised) shading is included in the result texture, as the rest of shading is stored in
    // the denoising AOVs below.
    result.rgb            = gFrameData.isDenoisingEnabled ? output.direct : result.rgb;
    gResult[screenCoords] = result;

    // Store the NDC depth value, if enabled. The value is only stored if the first sample was
    // computed (i.e. there is no previously stored value), or it is less than the previously stored
    // value.
    if (gFrameData.isDepthNDCEnabled && (sampleIndex == 0 || output.depthNDC < gDepthNDC[screenCoords]))
    {
        gDepthNDC[screenCoords] = output.depthNDC;
    }

    // Prepare and store the data for the denoising AOVs, if enabled.
    // NOTE: Only the data for the last sample is stored, with the expectation that the sample count
    // will be one when these AOVs are enabled.
    if (gFrameData.isDenoisingAOVsEnabled)
    {
        // Remap the normal components from [-1.0, 1.0] to [0.0, 1.0] for unsigned normalized
        // output.
        float3 normalEncoded = (output.normal + 1.0f) * 0.5f;

        // Prepare the diffuse and glossy radiance and hit distances. The hit distances are
        // normalized relative to the scene size, so they are always in [0.0, 1.0].
        float diffuseHitDist    = saturate(output.indirect.diffuseHitDist / gFrameData.sceneSize);
        float glossyHitDist     = saturate(output.indirect.glossyHitDist / gFrameData.sceneSize);
        float4 diffusePacked =
            float4(output.indirect.diffuse, diffuseHitDist);             // TODO: use NRD packing
        float4 glossyPacked = float4(output.indirect.glossy, glossyHitDist); // TODO: use NRD packing

        // Store the data for the denoising AOVs.
        gDepthView[screenCoords]          = output.depthView;
        gNormalRoughness[screenCoords]    = float4(normalEncoded, output.roughness);
        gBaseColorMetalness[screenCoords] = float4(output.baseColor, output.metalness);
        gDiffuse[screenCoords]            = diffusePacked;
        gGlossy[screenCoords]             = glossyPacked;
    }
}

// =================================================================================================
// Shadow miss shader
// =================================================================================================

// Shadow miss results sets the visibility to WHITE.
[shader("miss")] void ShadowMissShader(inout ShadowRayPayload rayPayload)
{
    rayPayload.visibility = WHITE;
}

// =================================================================================================
// Instance closest-hit shader
// =================================================================================================

// Closest hit shader for instance rays.  Just returns the shading data.
[shader("closesthit")] void InstanceClosestHitShader(
    inout InstanceRayPayload rayPayload, in BuiltInTriangleIntersectionAttributes hit)
{

    // Get the  interpolated vertex data for the hit triangle, at the hit barycentric coordinates.
    uint triangleIndex = PrimitiveIndex();

    // Compute barycentrics for hit.
    float3 barycentrics = computeBarycentrics(hit);

    // Set the instance index.
    rayPayload.instanceIndex = InstanceID();
    
    // Return the shading data in the ray payload.
    ShadingData shading = computeShadingData(gGeometry, triangleIndex, hit.barycentrics, ObjectToWorld3x4());
    rayPayload.geomPosition = shading.geomPosition;
    rayPayload.texCoord = shading.texCoord;
    rayPayload.objectNormal = shading.objectNormal;
    rayPayload.objectPosition = shading.objectPosition;
    rayPayload.indices = shading.indices;
    rayPayload.bary2 = hit.barycentrics;
    
    // Return the ray distance from the collision t.
    rayPayload.distance = RayTCurrent();

    // Return the offset within global instance buffer.
    rayPayload.instanceBufferOffset = gInstanceBufferOffset;
}

[shader("anyhit")] void InstanceShadowAnyHitShader(
        inout ShadowRayPayload rayPayload, in BuiltInTriangleIntersectionAttributes hit)
{

    // If the material is opaque, set the visibility to zero, accept the hit, and stop searching for
    // hits, as the shadow ray is completely blocked. Doing this here is a performance optimization,
    // as it avoids calling the full material initialization below, which can be expensive.
    if (gIsOpaque)
    {
        rayPayload.visibility = BLACK;
        AcceptHitAndEndSearch();
    }

    // Get the interpolated vertex data for the hit triangle, at the hit barycentric coordinates.
    uint triangleIndex = PrimitiveIndex();
    ShadingData shading =
        computeShadingData(gGeometry, triangleIndex, hit.barycentrics, ObjectToWorld3x4());

    // Get material buffer offset from instance data header.
    int materialBufferOffset = instanceMaterialBufferOffset(gInstanceBufferOffset);
    
    // Retrieve the shader index for intersected surface material.
    // This is stored in the material header in the global material byte buffer,
    // at a position indicated by instance's gMaterialBufferOffset.
    int shaderIndex = materialShaderIndex(materialBufferOffset);
    
    // Normal parameters generated by material's evaluate function (not used in shadow any hit shader.)
    float3 materialNormal = shading.normal;
    bool isGeneratedNormal = false;

    // Initialize the material using the shader index with the externally defined evaluate function.
    // This function will be generated at runtime and linked with the rest of the code.
    Material material = evaluateMaterialForShader(shaderIndex, shading,
                            materialBufferOffset, materialNormal, isGeneratedNormal);

    // Compute opacity from material opacity and transmission.
    float3 opacity =
        material.opacity * (WHITE - material.transmission * material.transmissionColor);
    rayPayload.visibility *= 1.0f - opacity;

    // If the visibility is zero (opaque) at this point, accept this hit and stop searching for
    // hits, as the shad ray is now completely blocked. Otherwise, ignore the hit so that
    // visibility can continue to be accumulated from other any hit intersections.
    if (isBlack(rayPayload.visibility))
    {
        AcceptHitAndEndSearch();
    }
    else
    {
        IgnoreHit();
    }

    // NOTE: The default behavior of accepting the hit while continuing to search for hits is not
    // useful for shadow rays. It does not happen here, because of the intrinsic function calls
    // above.

}
