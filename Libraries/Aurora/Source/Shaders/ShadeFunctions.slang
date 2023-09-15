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
#include "BSDF.slang"

// Compute shading from material emission.
float3 shadeEmission(Material material)
{
    // Compute emission from the material properties. The emission is affected by the coat, i.e. the
    // coat is on top of a glowing layer.
    float3 emissionCoatBlend = lerp(WHITE, material.coatColor, material.coat);
    float3 emission          = material.emission * material.emissionColor * emissionCoatBlend;

    return emission;
}

// Compute shading with a directional light.
float3 shadeDirectionalLight(float3 dir, float4 colorAndIntensity, float cosRadius,
    RaytracingAccelerationStructure scene, Material material, ShadingData shading, float3 V,
    bool onlyOpaqueShadowHits, int depth, int maxDepth, inout Random rng)
{
    // Perform shading with the global directional light. Treat the light as having a disc area by
    // sampling a random direction in a cone around the light direction.
    float2 random = random2D(rng);
    float3 L      = sampleCone(random, dir, cosRadius);

    // Evaluate the BSDF of the material, using the view direction and light direction.
    float3 bsdfAndCosine = evaluateMaterial(material, shading, V, L);
    if (isBlack(bsdfAndCosine))
    {
        return BLACK;
    }

    // Compute the light radiance from the light properties, and use a shadow ray to compute the
    // visibility of the light source at the hit position. If the view (ray) direction is on the
    // back side of the geometry, use the geometric position to avoid self-intersection.
    float3 lightRadiance = colorAndIntensity.a * colorAndIntensity.rgb;
    float3 position      = dot(V, shading.normal) < 0.0f ? shading.geomPosition : shading.position;
    float3 lightVisibility =
        traceShadowRay(scene, position, L, M_RAY_TMIN, onlyOpaqueShadowHits, depth, maxDepth);

    // Compute the outgoing radiance as the BSDF (with cosine term) multipled by the light
    // visibility and radiance.
    return lightVisibility * lightRadiance * bsdfAndCosine;
}

// Compute shading with an environment light as direct lighting.
float3 shadeEnvironmentLightDirect(Environment environment,
    RaytracingAccelerationStructure scene, Material material, ShadingData shading, float3 V,
    bool onlyOpaqueShadowHits, int depth, int maxDepth, inout Random rng)
{
    // Sample the environment to determine a sample (light) direction and the corresponding
    // probability density function (PDF) value for that direction.
    float3 L;
    float pdf;
    float2 random        = random2D(rng);
    float3 lightRadiance = sampleEnvironment(environment, random, L, pdf);

    // Evaluate the BSDF of the material, using the view direction and light direction.
    float3 bsdfAndCosine = evaluateMaterial(material, shading, V, L);
    if (isBlack(bsdfAndCosine))
    {
        return BLACK;
    }

    // Use a shadow ray to compute the visibility of the environment at the hit position. If the
    // view (ray) direction is on the back side of the geometry, use the geometric position to avoid
    // self-intersection.
    float3 position = dot(V, shading.normal) < 0.0f ? shading.geomPosition : shading.position;
    float3 lightVisibility =
        traceShadowRay(scene, position, L, M_RAY_TMIN, onlyOpaqueShadowHits, depth, maxDepth);

    // Compute the outgoing radiance as the BSDF (with cosine term) multipled by the light
    // visibility and radiance, divided by the PDF.
    return lightVisibility * lightRadiance * bsdfAndCosine / pdf;
}

// Compute shading with an environment light using multiple importance sampling.
float3 shadeEnvironmentLightMIS(Environment environment, RaytracingAccelerationStructure scene, Material material,
        ShadingData shading, float3 V, bool onlyOpaqueShadowHits, int depth, int maxDepth,
        inout Random rng, inout IndirectOutput indirect)
{
    float3 result = BLACK;

    // Generate random numbers for sampling.
    float2 random = random2D(rng);

    // If the view (ray) direction is on the back side of the geometry, use the geometric position
    // for shadow rays to avoid self-intersection. Otherwise use the shading normal.
    float3 position = dot(V, shading.normal) < 0.0f ? shading.geomPosition : shading.position;

    // Multiple importance sampling (MIS) proceeds as follows:
    // 1) Sample the *material* BSDF-and-cosine to get a new light direction and calculate the PDF.
    // 2) Evaluate the light in the light direction and calculate the PDF.
    // 3) Trace a shadow ray in the light direction to compute visibility.
    // 4) Weight the outgoing radiance with a balance heuristic and store that result.
    // 5) Sample the *light* and calculate the PDF.
    // 6) Evaluate the material BSDF-and-cosine in the light direction and calculate the PDF.
    // 7) Trace a shadow ray in the light direction to compute visibility.
    // 8) Weight the outgoing radiance with a balance heuristic and that to the result from step #4.

    // Step 1: Sample the BSDF of the material, using the view direction. In addition to the BSDF
    // value, this computes a sample (light) direction and the corresponding PDF value for that
    // direction.
    float3 L;
    float materialPDF;
    int lobeID;
    float3 bsdfAndCosine =
        sampleMaterial(material, shading, V, random, L, materialPDF, lobeID);

    // If the sampled lobe is transmission, simply return black. Transmission is handled separately,
    // and does not repond to lighting.
    if (lobeID == TRANSMISSION_LOBE)
    {
        return BLACK;
    }

    // Step 2: Evaluate the environment for the light direction.
    float lightPDF;
    float3 lightRadiance = evaluateEnvironment(environment, L, lobeID == TRANSMISSION_LOBE);

    // Skip the following steps if the light radiance or BSDF-and-cosine is black.
    if (!isBlack(lightRadiance) && !isBlack(bsdfAndCosine))
    {
        // Step 2 (PDF): Calculate the light PDF for the light direction, by dividing the luminance
        // of the radiance by the integral of the luminance over the entire environment. If the
        // environment is not from an image, use the PDF for uniform sampling over all directions.
        lightPDF = environment.constants.hasLightTex
            ? (luminance(lightRadiance) / environment.constants.lightTexLuminanceIntegral)
            : (0.25f * M_PI_INV);

        // Step 3: Use a shadow ray to compute the visibility of the light at the hit position.
        float3 lightVisibility =
            traceShadowRay(scene, position, L, M_RAY_TMIN, onlyOpaqueShadowHits, depth, maxDepth);

        // Step 4: Compute a weight based on the PDF of the material and the light, and apply that
        // weight to the outgoing radiance: visibility * radiance * BSDF * cosine / material PDF.
        // Add that to the prior weighted result.
        float weight = computeMISWeight(1, materialPDF, 1, lightPDF);
        result += weight * lightVisibility * lightRadiance * bsdfAndCosine / materialPDF;
    }

    // Step 5: Sample the environment to determine a sample (light) direction and the corresponding
    // probability density function (PDF) value for that direction.
    lightRadiance = sampleEnvironment(environment, random, L, lightPDF);

    // Step 6: Evaluate the BSDF of the material, using the view direction and light direction, and
    // calculate the material PDF for the light direction.
    int evaluatedLobeID;
    bsdfAndCosine = evaluateMaterialAndPDF(material, shading, V, L, random, materialPDF, evaluatedLobeID);

    // Skip the following steps if the light radiance or BSDF-and-cosine is black.
    if (!isBlack(lightRadiance) && !isBlack(bsdfAndCosine))
    {
        // Step 7: Use a shadow ray to compute the visibility of the light at the hit position.
        float3 lightVisibility =
            traceShadowRay(scene, position, L, M_RAY_TMIN, onlyOpaqueShadowHits, depth, maxDepth);

        // Step 8: Compute a weight based on the PDF of the light and the material, and apply that
        // weight to the outgoing radiance: visibility * radiance * BSDF * cosine / light PDF.
        float weight = computeMISWeight(1, lightPDF, 1, materialPDF);
        result += weight * lightVisibility * lightRadiance * bsdfAndCosine / lightPDF;
    }

    // Update the indirect output values based on the sampled lobe at Step 1: diffuse for the
    // diffuse lobe, and treat everything else as glossy. As this is an environment light, the hit
    // distances are set to infinity.
    // NOTE: This works because the material sampling (Step 1) and evaluation (Step 6) are using the
    // same random numbers and therefore are expected to use the same lobe. An alternative is for
    // Step 6 to perform a complete evaluation (rather than selecting a single lobe) and combine the
    // PDFs of the lobes, with separate diffuse and glossy components returned from the evaluation.
    if (lobeID == DIFFUSE_LOBE)
    {
        indirect.diffuse += result;
        indirect.diffuseHitDist = INFINITY;
    }
    else
    {
        indirect.glossy += result;
        indirect.glossyHitDist = INFINITY;
    }

    return result;
}

// Compute shading with indirect light, from a random direction.
int setupIndirectLightRay(Material material, ShadingData shading, float3 V, int depth,
    int maxDepth, inout Random rng, out float3 indirectRayDirec, out float3 indirectRayOrigin, out float3 indirectColorOut)
{
    // Generate random numbers for sampling.
    float2 random = random2D(rng);

    // Sample the BSDF of the material, using the view direction. In addition to the BSDF value,
    // this computes a sample (light) direction and the corresponding probability density function
    // (PDF) value for that direction.
    float3 L;
    float pdf;
    int lobeID;
    float3 bsdfAndCosine =
        sampleMaterial(material, shading, V, random, L, pdf, lobeID);
    if (isBlack(bsdfAndCosine))
    {
        return NO_SECONDARY_RAY;
    }

    // Trace a radiance ray with the sampled light direction, i.e. the next step in tracing the
    // current path. If the view (ray) direction is on the back side of the geometry, or the ray is
    // from a transmission lobe, use the geometric position to avoid self-intersection. Also, if the
    // ray is from a transmission lobe, use the background miss shader so that the background is
    // sampled.
    bool isTransmission = lobeID == TRANSMISSION_LOBE;
    indirectRayOrigin =
        (dot(V, shading.normal) < 0.0f || isTransmission) ? shading.geomPosition : shading.position;
    indirectRayDirec = L;
    indirectColorOut = bsdfAndCosine / pdf;
    return lobeID;
}
