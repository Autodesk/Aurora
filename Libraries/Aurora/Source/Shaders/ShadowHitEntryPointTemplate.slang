// Copyright 2022 Autodesk, Inc.
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

// Any hit shader for shadow rays.
// NOTE: It is possible for this to be called multiple times for the same intersection, which would
// lead to incorrect visibility determination. However, this has not been a problem in practice.
// If this does become an issue, the D3D12_RAYTRACING_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION flag can
// be used, with a performance cost.
#include "PathTracingCommon.slang"
#include "InitializeMaterial.slang"
#include "ShadeFunctions.slang"

[shader("anyhit")] void @MATERIAL_TYPE@ShadowAnyHitShader(
    inout ShadowRayPayload rayPayload, in BuiltInTriangleIntersectionAttributes hit)
{
    // If the material is opaque, set the visibility to zero, accept the hit, and stop searching for
    // hits, as the shadow ray is completely blocked. Doing this here is a performance optimization,
    // as it avoids calling the full material initialization below, which can be expensive.
    if (gMaterialConstants.isOpaque)
    {
        rayPayload.visibility = BLACK;
        AcceptHitAndEndSearch();
    }

    // Get the interpolated vertex data for the hit triangle, at the hit barycentric coordinates.
    uint triangleIndex  = PrimitiveIndex();
    float3 barycentrics = computeBarycentrics(hit);
    ShadingData shading = computeShadingData(
        gGeometry, triangleIndex, barycentrics, ObjectToWorld3x4());

    // Initialize the material values.
    // NOTE: This evaluates all material properties, when only visibility (or opacity) is needed. It
    // may be more efficient to generate a dedicated function for getting the visibility alone.
    float3 materialNormal  = shading.normal;
    bool isGeneratedNormal = false;
    Material material      = initializeMaterial(shading, ObjectToWorld3x4(), materialNormal, isGeneratedNormal);

    // Compute the opacity at the current hit from the opacity, transmission, and transmission color
    // properties. Then accumulate that (inverted) with the current visibility on the ray payload.
    // NOTE: Visibility accumulation like this is order-independent, i.e. it does not matter what
    // order intersections are processed. This is important because any hit shader intersections are
    // processed in an arbitrary order. Also, while this does consider *transmission* to determine
    // visibility, this technique does not support refraction (caustics), just simple straight
    // shadow rays.
    float3 opacity =
        material.opacity * (WHITE - material.transmission * material.transmissionColor);
    rayPayload.visibility *= 1.0f - opacity;

    // If the visibility is zero (opaque) at this point, accept this hit and stop searching for
    // hits, as the shadow ray is now completely blocked. Otherwise, ignore the hit so that
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
