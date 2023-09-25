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

#include "BSDFCommon.slang"
#include "Colors.slang"
#include "Environment.slang"
#include "Globals.slang"
#include "Random.slang"
#include "RayTrace.slang"
#include "GlobalPipelineState.slang"

// TODO: Re-enable ground plane.

// Shades a matte reflection effect with the specified properties.
int setupMatteReflectionIndirectLightRay(float opacity, float3 color, float roughness, float3 rayDir, float3 normal,
    float3 tangent, float3 bitangent, float3 position, uint maxDepth, inout Random rng, out float3 indirectRayDirec, out float3 indirectRayOrigin, out float3 indirectColorOut)
{
    // Transform the ray to a view direction in tangent space.
    float3 V = worldToTangent(-rayDir, tangent, bitangent, normal);

    // Sample a GGX-Smith BRDF with the f0 value set to the reflection color, to represent a metal
    // material. This gives a BSDF result, which is scaled by the cosine term.
    float3 L;
    float pdf;
    float3 f0 = color;
    float2 random = random2D(rng);
    BSDF_AND_COSINE bsdfAndCosine = sampleGGXSmithBRDF(V, f0, roughness, 0.0f, random, L, pdf);
    bsdfAndCosine *= abs(L.z);
    if (isBlack(bsdfAndCosine))
    {
        return NO_SECONDARY_RAY;
    }

    // Trace a radiance ray with the sampled light direction. If the ray payload alpha is zero,
    // treat this as a miss, i.e. there is no matte reflection of the environment. Otherwise,
    // compute the outgoing radiance as the BSDF (with cosine term) multipled by the light radiance,
    // divided by the PDF.
    float3 Lw = tangentToWorld(L, tangent, bitangent, normal);
    
    // Compute the indirect ray, i.e. the next step in tracing the current path. 
    
    // If the view (ray) direction is on the back side of the geometry, or the ray is
    // from a transmission lobe, use the geometric position to avoid self-intersection. 
    indirectRayOrigin = position;
    
    // Set the direction from the sampled light direction.
    indirectRayDirec = Lw;
    
    // Set the color from the sampled color scaled by the inverse of the PDF.
    indirectColorOut = bsdfAndCosine / pdf;
    
    return GROUND_PLANE_REFLECTION_RAY;
}

// Shades a matte shadow effect with the specified environment light and position.
// TODO: This is an approximation that avoids the use of extra buffers that are accumulated later,
// i.e. shadowed and unshadowed components. This also does not consider the directional light. Both
// of these issues should be addressed in the future.
float4 shadeMatteShadow(float opacity, float3 color, RaytracingAccelerationStructure scene,
    Environment environment, float3 normal, float3 position, bool onlyOpaqueShadowHits, uint maxDepth,
    inout Random rng)
{
    // Sample the environment to determine a sample (light) direction and the corresponding
    // probability density function (PDF) value for that direction.
    float3 L;
    float pdf;
    float2 random = random2D(rng);
    float3 lightRadiance = sampleEnvironment(environment, random, L, pdf);

    // If the light direction is below the ground plane, the light does not contribute to shadowing.
    if (dot(L, normal) < 0.0f)
    {
        return BLACK;
    }

    // Use a shadow ray to compute the visibility of the light source at the hit position.
    float3 lightVisibility = traceShadowRay(scene, position, L, M_RAY_TMIN, onlyOpaqueShadowHits, 1, maxDepth);

    // The result color is the shadow color, and the alpha (blend factor) is the luminance of the
    // inverse light visibility (i.e. zero visibility means full shadow) scaled by the shadow
    // opacity.
    float4 result = BLACK;
    result.rgb = color;
    result.a = luminance(1.0f - lightVisibility) * opacity;
    
    // Return result color.
    return result;
}

bool intersectGroundPlane(GroundPlane groundPlane, float3 rayOrigin, float3 rayDir, inout float3 hitPosition, inout float distance)
{
    // Collect ground plane properties.
    float3 position = groundPlane.position;
    float3 normal = groundPlane.normal;
    
    // Compute the dot product of the ray direction and plane normal. If this is greater than zero,
    // the ray direction is below the plane and the plane is not visible.
    float planeDot = dot(rayDir, normal);
    if (planeDot >= 0.0)
    {
        return false;
    }
    
    // Compute the distance to the plane along the ray direction, i.e ray-plane intersection. If the
    // distance is less than zero (behind the ray) or greater than the maximum distance, the plane
    // is not visible.
    distance = dot(position - rayOrigin, normal) / planeDot;
    if (distance < 0.0)
        return false;
    
    // Compute the hit position.
    hitPosition = rayOrigin + rayDir * distance;
    
    return true;
    
}

// Shades the specified ground plane for the specified ray. The result is intended to be blended
// with whatever is behind the ground plane. This returns black with zero alpha if the ground plane
// is not hit.
int setupGroundPlaneRay(float3 hitPosition, GroundPlane groundPlane, RaytracingAccelerationStructure scene,
    Environment environment, float3 rayOrigin, float3 rayDir,
    bool isForceOpaqueShadowsEnabled, uint maxDepth, inout Random rng,
    out float3 groundPlaneRayDirecOut, out float3 groundPlaneRayOriginOut, out float3 groundPlaneColorOut)
{
    // Collect ground plane properties.
    float3 position = groundPlane.position;
    float3 normal = groundPlane.normal;
    float3 tangent = groundPlane.tangent;
    float3 bitangent = groundPlane.bitangent;
    float shadowOpacity = groundPlane.shadowOpacity;
    float3 shadowColor = groundPlane.shadowColor;
    float reflectionOpacity = groundPlane.reflectionOpacity;
    float3 reflectionColor = groundPlane.reflectionColor;
    float reflectionRoughness = groundPlane.reflectionRoughness;
   
    // Shade with a matte reflection layer, if enabled.
    groundPlaneColorOut = WHITE;
    if (reflectionOpacity > 0.0f)
    {
        // Compute transparency from inverse of reflection opacity.
        float3 transparency = 1.0f - reflectionOpacity;
        
        // Randomly choose if next ray is a reflection ray, based on reflection opacity.
        float Pr = luminance(transparency);
        if (random2D(rng).x >= Pr)
        {
            // Setup the next ray as a reflection ray.
            int rayType = setupMatteReflectionIndirectLightRay(reflectionOpacity, reflectionColor, reflectionRoughness,
                rayDir, normal, tangent, bitangent, hitPosition, maxDepth, rng,
                groundPlaneRayDirecOut, groundPlaneRayOriginOut, groundPlaneColorOut);
            
            // Normalize based on probability of reflecting.
            groundPlaneColorOut *= reflectionOpacity / (1.0f - Pr);
            
            // Return the reflection ray type.
            return rayType;
        }
        else
            groundPlaneColorOut = transparency / Pr; // Normalize based on probability of not reflecting.
    }
    
    // Shade with a matte shadow layer, if enabled.    
    if (shadowOpacity > 0.0f)
    {
        // Shade ground plane shadow.
        float4 shadow = shadeMatteShadow(shadowOpacity, shadowColor, scene, environment, normal,
                hitPosition, true, maxDepth, rng);
        
        // Compute interpolated shadow value (ground plane shadow color in shadowed regions, white elsewhere.)
        float3 interpolatedShadowColor = lerp(WHITE, shadow.rgb, shadow.a);
        
        // Scale returned color by shadow value.
        groundPlaneColorOut *= interpolatedShadowColor;

    }
    
    return CAMERA_RAY;
}

