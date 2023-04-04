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

// Entry point template that defines a hit shader for a material type.
// NOTE: This file is not valid HLSL as is. This template must be configured at runtime by
// replacing the tags surrounded by the @ character:
// -@MATERIAL_TYPE@ with the unique material type name for this shader.

// Closest hit shader for radiance rays.
#include "PathTracingCommon.slang"
#include "InitializeMaterial.slang"
#include "ShadeFunctions.slang"

[shader("closesthit")] void @MATERIAL_TYPE@RadianceHitShader(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes hit)
{
    int depth                   = rayPayload.radianceRay.depth;
    int maxDepth                = gFrameData.traceDepth;
    bool isOpaqueShadowsEnabled = gFrameData.isOpaqueShadowsEnabled;

    // Get the interpolated vertex data for the hit triangle, at the hit barycentric coordinates.
    uint triangleIndex  = PrimitiveIndex();
    float3 barycentrics = computeBarycentrics(hit);
    ShadingData shading = computeShadingData(
        gGeometry, triangleIndex, barycentrics, ObjectToWorld3x4());

    // Initialize the view direction.
    float3 V              = -WorldRayDirection();
    float3 materialNormal = shading.normal;
    bool isGeneratedNormal;

    // Initialize the material values for repeated evaluations at the current shading point. Also
    // initialize the environment.
    Material material = initializeMaterial(
        shading, 
        ObjectToWorld3x4(), 
        materialNormal, 
        isGeneratedNormal);
    Environment environment = prepareEnvironmentValues();

    // If a new normal has been generated, transform it to world space and build a corresponding
    // basis from it.
    if (isGeneratedNormal)
    {
        shading.normal = materialNormal;
        buildBasis(shading.normal, shading.tangent, shading.bitangent);
    }

    // Modify the material properties if only the diffuse component should be renderered.
    if (gFrameData.isDiffuseOnlyEnabled)
    {
        material.base      = 1.0f;
        material.specular  = 0.0f;
        material.metalness = 0.0f;
    }

    // Shade any material layers (for front facing hits only) if the ENABLE_LAYERS option is
    // defined.
    // TODO: Should this be done after the transparency evaluation below, i.e. it is possible this
    // work will be thrown away for transparent samples.
#if ENABLE_LAYERS
    if (dot(shading.normal, V) > 0.0)
    {
        // Iterate in reverse order, so innermost layer is shaded last.
        for (int layer = gMaterialLayerCount - 1; layer >= 0; layer--)
        {
            int layerMissShaderIdx = getMaterialLayerIndex(layer);

            // Shade the layer, and return if the ray was absorbed.
            if (shadeMaterialLayer(gNullScene, layerMissShaderIdx, rayPayload.radianceRay, shading,
                    hit, depth, maxDepth))
            {
                return;
            }
        }
    }
#endif

    // Handle opacity (transparency) by stochastically determining whether the hit should be
    // skipped, based on the luminance of the material opacity. If so, use the ray payload from a
    // ray traced at the hit point, i.e. passing straight through the geometry, and return.
    // NOTE: This could also be done with an any hit shader, with possibly better or worse
    // performance.
    float3 transparency = 1.0f - material.opacity;
    float P             = luminance(transparency);
    if (random2D(rayPayload.radianceRay.rng).x < P)
    {
        // Trace a radiance ray with the unmodified ray direction, and the hit position. Use the
        // background miss shader so that the background is sampled, since this is for transparency.
        // NOTE: The geometric position (geomPosition) is used to avoid self-intersection.
        rayPayload.radianceRay = traceRadianceRay(gScene, environment, shading.geomPosition, -V,
            M_RAY_TMIN, depth, maxDepth, true, rayPayload.radianceRay.rng);

        // Scale the color components of the ray payload by transparency, and normalized by the
        // probability of this segment being considered transparent.
        rayPayload.radianceRay.scaleColor(transparency / P);

        // Nothing more to do.
        return;
    }

    // Compute the NDC depth and view depth of the hit position:
    // - NDC depth: Compute the clip space position using the supplied view-projection matrix, then
    //   divide the Z component by W, and remap from [-1.0, 1.0] to [0.0, 1.0].
    // - View Depth: For a primary ray, this is simply the T value of the ray. We don't use this
    //   value for non-primary rays; it is recorded here but not used.
    float4 positionClip = mul(gFrameData.cameraViewProj, float4(shading.geomPosition, 1.0f));
    float depthNDC      = (positionClip.z / positionClip.w + 1.0f) / 2.0f;
    float depthView     = RayTCurrent();

    // Clamp roughness for the ray payload to a minimum, because the denoiser handles materials with
    // low (near-zero) roughness poorly, leading to a noisy result. This addresses two separate but
    // related issues: low-roughness metallic materials reflecting noisy surroundings, and low-
    // roughness dielectric materials where the glossy lobe is sparsely sampled.
    // NOTE: The roughness in the ray payload is used for denoising only. The material specular
    // roughness (used in shading) is not modified here.
    static const float kMinRoughness = 0.05f;
    float clampedRoughness           = max(material.specularRoughness, kMinRoughness);

    // Store initial data in the ray payload.
    rayPayload.radianceRay.color     = BLACK;
    rayPayload.radianceRay.alpha     = 1.0f;
    rayPayload.radianceRay.direct    = BLACK;
    rayPayload.radianceRay.depthNDC  = depthNDC;
    rayPayload.radianceRay.depthView = depthView;
    rayPayload.radianceRay.normal    = shading.normal;
    rayPayload.radianceRay.baseColor = material.baseColor;
    rayPayload.radianceRay.roughness = clampedRoughness;
    rayPayload.radianceRay.metalness = material.metalness;
    rayPayload.radianceRay.indirect.clear();

    // Shade with the global directional light. Skip this if the light intensity is zero.
    if (gFrameData.lightColorAndIntensity.a > 0.0f)
    {
        rayPayload.radianceRay.direct = shadeDirectionalLight(gFrameData.lightDir,
            gFrameData.lightColorAndIntensity, gFrameData.lightCosRadius, gScene, material, shading,
            V, isOpaqueShadowsEnabled, depth, maxDepth, rayPayload.radianceRay.rng);

        // Add the direct lighting to the color result.
        // NOTE: For denoising purposes, "direct" lighting only includes the directional light, and
        // not the environment light computed below.
        rayPayload.radianceRay.color += rayPayload.radianceRay.direct;
    }

    // When denoising, primary rays for indirect lighting should not include the base color (also
    // known as albedo, as used by diffuse lobes) in order to avoid blurring out color details. So
    // the base color is set to white here and the color is added back after denoising.
    if (gFrameData.isDenoisingEnabled && rayPayload.radianceRay.depth == 1)
    {
        material.baseColor = WHITE;
    }

    // Shade with the environment light depending on the importance sampling type:
    // - BSDF: The environment is treated as indirect light, and is evaluated in the miss shader.
    // - Environment: Sample the environment light as direct lighting.
    // - MIS: Use multiple importance sampling on both the material and the light.
    // NOTE: For denoising purposes, "indirect" lighting includes the environment light.
    if (IMPORTANCE_SAMPLING_MODE != IMPORTANCE_SAMPLING_BSDF)
    {
        float3 environmentRadiance = BLACK;
        if (IMPORTANCE_SAMPLING_MODE == IMPORTANCE_SAMPLING_ENVIRONMENT)
        {
            // TODO: This does not currently contribute to indirect lighting for denoising purposes.
            // That will require having the material evaluation return separate diffuse and glossy
            // components.
            environmentRadiance =
                shadeEnvironmentLightDirect(environment, gScene, material, shading,
                    V, isOpaqueShadowsEnabled, depth, maxDepth, rayPayload.radianceRay.rng);
        }
        else if (IMPORTANCE_SAMPLING_MODE == IMPORTANCE_SAMPLING_MIS)
        {
            environmentRadiance = shadeEnvironmentLightMIS(environment, gScene, material, shading, V, isOpaqueShadowsEnabled, depth,
                    maxDepth, rayPayload.radianceRay.rng, rayPayload.radianceRay.indirect);
        }

        // Add the radiance from the environment light to the color result.
        rayPayload.radianceRay.color += environmentRadiance;
    }

    // Shade with indirect light from the surrounding scene (i.e. path tracing).
    rayPayload.radianceRay.color += shadeIndirectLight(
        gScene, environment, material, shading, V, depth, maxDepth, rayPayload.radianceRay.rng,
        rayPayload.radianceRay.alpha, rayPayload.radianceRay.indirect);

    // Scale the color components of the ray payload by opacity, and normalized by the probability
    // of this segment being considered opaque.
    // NOTE: The shading functions do not individually consider opacity, so that it can be handled
    // in one place here.
    rayPayload.radianceRay.scaleColor(material.opacity / (1.0f - P));
}
