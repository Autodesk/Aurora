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
#ifndef __BSDFCOMMON_H__
#define __BSDFCOMMON_H__

#include "Globals.slang"
#include "Sampling.slang"

// A typedef for the result of a BSDF, i.e. the ratio of outgoing radiance to incoming irradiance.
// Some functions include the cosine term with the BSDF, so another definition is used for clarity.
#define BSDF float3
#define BSDF_AND_COSINE float3

// Integer lobe IDs used for debug purposes.
// NOTE: These are in order defined by the closure tree in the Standard Surface specification. The
// names used there are shown as EOL comments below. https://autodesk.github.io/standard-surface
#define TRANSPARENCY_LOBE 1 // transparency
#define COAT_LOBE 2         // coat_brdf
#define METAL_LOBE 3        // metal_brdf
#define GLOSSY_LOBE 4       // spec_brdf
#define TRANSMISSION_LOBE 5 // spec_btdf
#define SHEEN_LOBE 6        // sheen_brdf
#define DIFFUSE_LOBE 7      // diffuse_brdf

// Ray types (in addition to the lobe IDs defined above.)
#define NO_SECONDARY_RAY    -1
#define CAMERA_RAY         -2
#define GROUND_PLANE_REFLECTION_RAY         -3

// =================================================================================================
// Vector Conventions
// =================================================================================================
//
// The following names are typically used for vectors:
// - V: The view direction. This is away from the shading point, or toward the viewer. This is
//   sometimes called the outgoing direction or "wo" in research.
// - L: The light direction. This is away from the shading point, or toward the light. This is
//   sometimes called the incoming direction or "wi" in research.
// - N, X, Y: The normal, tangent, and bitangent vectors. These usually refer to a surface point.
//   Note that the name "binormal" in inaccurate and should not be used.
// - H: The half-vector between two other vectors. This usually refers to a microfacet normal, or
//   "m" in research. It can be constructed from V and L vectors as H = normalize(V + L).
//
// The following conventions apply to the use of vectors for shading:
// - BSDFs are called with and return tangent-space vectors. The caller must convert to / from
//   world-space vectors as needed with worldToTangent() and tangentToWorld(). This simplifies
//   computation, e.g. you can use V.z in tangent space instead of dot(V, N).
// - Materials must handle the possibility that V and L are in opposite hemispheres. Specifically,
//   use a BRDF (reflection) when they are in the same hemisphere, and use a BTDF (transmission)
//   otherwise. This is not detected in the BSDF evaluation functions here, but is handled by the
//   BSDF sampling functions that call the evaluation functions.
// - From this, double-sided shading is supported by simply using the absolute value of any dot
//   products involving the shading normal N, instead of testing or clamping.
// - The normal direction should not be flipped, so that it can used to determine the "outside" of
//   the surface, and therefore whether directions are entering (from the outside) or exiting (from
//   the inside).
// - Sampled directions that rely on a canonical (0.0, 0.0, 1.0) normal must be flipped as needed,
//   depending on the view direction.
// - See PBRT 3E 9.1 for more information on these conventions.

// =================================================================================================
// Utility Functions
// =================================================================================================

// Remaps a random variable to [0.0, 1.0] after passing the specified test value.
float remapPass(float random, float test)
{
    return random / test;
}

// Remaps a random variable to [0.0, 1.0] after failing the specified test value.
float remapFail(float random, float test)
{
    return (random - test) / (1.0f - test);
}

// Transforms the specified input vector in world space to the tangent space defined by [X, Y, N].
float3 worldToTangent(float3 input, float3 X, float3 Y, float3 N)
{
    return float3(dot(input, X), dot(input, Y), dot(input, N));
}

// Transforms the specified input vector in tangent space defined by [X, Y, N] to world space.
float3 tangentToWorld(float3 input, float3 X, float3 Y, float3 N)
{
    return input.x * X + input.y * Y + input.z * N;
}

// Returns whether the two tangent-space vectors in the same hemisphere.
bool sameHemisphere(float3 a, float3 b)
{
    return a.z * b.z > 0.0f;
}

// Converts an index of refraction (IOR) to reflectance at normal incidence (F0).
// NOTE: The standard dielectric IOR of 1.5 yields 4%.
float3 iorToF0(float ior)
{
    return sqr((1.0f - ior) / (1.0f + ior));
}

// Evaluates the Frensel equation for a conductor with the specified f0 value (i.e. reflectance at
// normal incidence), using the Schlick approximation.
float3 evaluateSchlickFresnel(float3 f0, float cosTheta)
{
    return lerp(f0, 1.0f, pow(1.0f - cosTheta, 5.0f));
}

// Computes a refraction direction from the specified tangent-space view direction and IOR. This
// also returns whether total internal reflection occurs.
float3 computeRefraction(float3 V, float ior, out bool reflected)
{
    reflected = false;

    // If the view direction (toward the viewer) is above the normal, then we assume that the ray is
    // *entering* the material with the specified IOR from the outside (air). Otherwise it is
    // *leaving* the material.
    bool isEnteringMaterial = V.z > 0.0f;

    // Invert the tangent-space normal if the ray is exiting the material. This is needed for the
    // refract() call to work as expected.
    float3 N = float3(0.0f, 0.0f, isEnteringMaterial ? 1.0f : -1.0f);

    // The refract() function expects an IOR ratio ("eta") as exiting IOR over entering IOR, so
    // compute that ratio based on whether the ray is entering the material or exiting it. The other
    // medium is assumed to be air (IOR 1.0).
    float eta = isEnteringMaterial ? 1.0f / ior : ior / 1.0f;

    // Compute the refraction direction from the ray direction and normal. If the refraction
    // direction has zero length, then perform total internal reflection (TIR) instead, returning
    // that reflection (true) was performed instead of refraction.
    float3 L = refract(-V, N, eta);
    if (length(L) == 0.0f)
    {
        // Compute a tangent-space reflection vector.
        L         = float3(-V.x, -V.y, V.z);
        reflected = true;
    }

    return L;
}

// =================================================================================================
// Lambertian BRDF
// =================================================================================================

// Evaluates the Lambertian diffuse BRDF, with the specified albedo.
float3 evaluateLambertianBRDF(float3 albedo)
{
    return albedo * M_PI_INV;
}

// Samples and evaluates the Lambertian diffuse BRDF, returning the BRDF value along with the
// sampled light direction and the PDF for that direction.
BSDF sampleLambertianBRDF(float3 V, float3 albedo, float2 random, out float3 L, out float pdf)
{
    // Sample a cosine-weighted direction from a hemisphere above the normal. If the view direction
    // is not in the same hemisphere as the normal, flip the normal before sampling to allow proper
    // shading of the backface.
    float3 N = float3(0.0f, 0.0f, V.z > 0.0f ? 1.0f : -1.0f);
    L        = sampleHemisphere(random, N, pdf);

    // Return black if the view and light directions are not in the same hemisphere.
    if (!sameHemisphere(V, L))
    {
        return BLACK;
    }

    // Evaluate the Lambertian BRDF.
    return evaluateLambertianBRDF(albedo);
}

// =================================================================================================
// GGX-Smith BRDF
// =================================================================================================
//
// This section refers to two papers by Eric Heitz:
// - 2014: Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.
// - 2018: Sampling the GGX Distribution of Visible Normals.

// Converts roughness to alpha for the GGX NDF and Smith geometry term.
// NOTE: See "Revisiting Physically Based Shading at Imageworks" from SIGGRAPH 2017.
float2 roughnessToAlpha(float roughness, float anisotropy)
{
    // Specify a minimum alpha value, as the GGX NDF does not support a zero alpha.
    static const float MIN_ALPHA = 0.0001f;

    // Square the roughness value and combine with anisotropy to produce X and Y alpha values.
    float alpha   = max(MIN_ALPHA, sqr(roughness));
    float2 alpha2 = float2(alpha * (1.0f + anisotropy), alpha * (1.0f - anisotropy));

    return alpha2;
}

// Evaluates the "lambda" expression used in Smith G1/G2 implementations, for the specified vector.
// NOTE: See 2018, equation #2.
float lambda(float3 vec, float2 alpha)
{
    float squared = (sqr(alpha.x) * sqr(vec.x) + sqr(alpha.y) * sqr(vec.y)) / sqr(vec.z);

    return (-1.0f + sqrt(1.0f + squared)) * 0.5f;
}

// Evaluates the Smith masking function (G1) for the specified view direction and alpha.
// NOTE: See 2014, equation #43.
float evaluateSmithG1(float3 V, float3 H, float2 alpha)
{
    // Return zero if the view direction is below the half-vector, i.e. fully masked.
    if (dot(V, H) <= 0.0f)
    {
        return 0.0f;
    }

    return 1.0f / (1.0f + lambda(V, alpha));
}

// Evaluates the Smith masking and shadowing function (G2) for the specified view and light
// directions and alpha.
// NOTE: See 2014, equation #43.
float evaluateSmithG2(float3 V, float3 L, float3 H, float2 alpha)
{
    // Return zero if the view or light direction is below the half-vector, i.e. fully masked and /
    // or shadowed.
    if (dot(V, H) <= 0.0f || dot(L, H) < 0.0f)
    {
        return 0.0f;
    }

    return 1.0f / (1.0f + lambda(V, alpha) + lambda(L, alpha));
}

// Evaluates the GGX NDF (D), for the given half-vector and alpha.
// NOTE: See 2018, equation #1.
float evaluateGGXNDF(float3 H, float2 alpha)
{
    float ellipse = sqr(H.x) / sqr(alpha.x) + sqr(H.y) / sqr(alpha.y) + sqr(H.z);

    return 1.0f / (M_PI * alpha.x * alpha.y * sqr(ellipse));
}

// Evaluates the GGX-Smith VNDF (Dv), for the given view direction, microfacet normal, and alpha.
// NOTE: See 2018, equation #1. This is used when sampling the GGX-Smith VNDF, and should not be
// used in place of the GGX NDF.
float evaluateGGXSmithVNDF(float3 V, float3 H, float2 alpha)
{
    // Return zero if the view direction is below the half-vector, i.e. fully masked.
    float VDotH = dot(V, H);
    if (VDotH <= 0.0f)
    {
        return 0.0f;
    }

    // Evaluate the D and G1 components for the VNDF.
    float D  = evaluateGGXNDF(H, alpha);
    float G1 = evaluateSmithG1(V, H, alpha);

    return D * G1 * VDotH / abs(V.z);
}

// Samples the GGX-Smith VNDF with the specified view direction, returning a microfacet normal and
// the PDF for that direction.
// NOTE: See 2018, appendix A.
float3 sampleGGXSmithVNDF(float3 V, float2 alpha, float2 random)
{
    // Section 3.2: Transform the view direction to the hemisphere configuration.
    float3 Vh = normalize(float3(alpha.x * V.x, alpha.y * V.y, V.z));

    // Section 4.1: Orthonormal basis (with special case if cross product is zero).
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1   = lensq > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) / sqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
    float3 T2   = cross(Vh, T1);

    // Section 4.2: Parameterization of the projected area.
    float r   = sqrt(random.x);
    float phi = 2.0f * M_PI * random.y;
    float t1  = r * cos(phi);
    float t2  = r * sin(phi);
    float s   = 0.5f * (1.0f + Vh.z);
    t2        = (1.0f - s) * sqrt(1.0f - t1 * t1) + s * t2;

    // Section 4.3: Reprojection onto hemisphere.
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0f - t1 * t1 - t2 * t2)) * Vh;

    // Section 3.4: Transform the normal back to the ellipsoid configuration.
    float3 H = normalize(float3(alpha.x * Nh.x, alpha.y * Nh.y, max(0.0f, Nh.z)));

    return H;
}

// Compute the PDF for the GGX-Smith VNDF, with the specified view direction and microfacet normal.
// NOTE: See 2018, appendix B.
float computeGGXSmithPDF(float3 V, float3 H, float2 alpha)
{
    // The PDF is simply the evaluation of the VNDF.
    return evaluateGGXSmithVNDF(V, H, alpha);
}

// Evaluates the GGX-Smith glossy BRDF, returning the BRDF value along with the Fresnel term used.
BSDF evaluateGGXSmithBRDF(
    float3 V, float3 L, float3 f0, float roughness, float anisotropy, out float3 F)
{
    // Convert roughness and anisotropy to alpha values.
    float2 alpha = roughnessToAlpha(roughness, anisotropy);

    // Compute the D (NDF), G (visibility), and F (Fresnel) terms, along with the microfacet BRDF
    // denominator.
    float3 H    = normalize(V + L);
    float D     = evaluateGGXNDF(H, alpha);
    float G2    = evaluateSmithG2(V, L, H, alpha);
    F           = evaluateSchlickFresnel(f0, dot(V, H));
    float denom = 4.0f * abs(V.z) * abs(L.z);

    // Return the combined microfacet expression.
    return D * G2 * F / denom;
}

// Samples and evaluates the GGX-Smith glossy BRDF, returning the BRDF value along with the sampled
// light direction and the PDF for that direction.
// NOTE: See 2018, appendix B.
BSDF sampleGGXSmithBRDF(float3 V, float3 f0, float roughness, float anisotropy, float2 random,
    out float3 L, out float pdf)
{
    // Convert roughness and anisotropy to alpha values.
    float2 alpha = roughnessToAlpha(roughness, anisotropy);

    // Sample the GGX-Smith VNDF to determine a microfacet normal (half-vector). If the view
    // direction is in the lower hemisphere in tangent space, flip the view direction normal before
    // sampling the VNDF, then flip the resulting microfacet normal.
    // NOTE: The VNDF expects the view direction to be in the upper hemisphere (with the normal).
    bool flip = V.z <= 0.0f;
    float3 H  = sampleGGXSmithVNDF(flip ? -V : V, alpha, random);
    H         = flip ? -H : H;

    // Reflect the view direction across the microfacet normal to get the sample direction.
    L = reflect(-V, H);

    // Compute the PDF, divided by a factor from using a reflected vector. See 2018 equation #17.
    // NOTE: 2018 equation #19 shows that a number of terms can be cancelled out to produce a simple
    // estimator: just F * G2 / G1. Note that the PDF and cosine terms from the rendering equation
    // are cancelled out. However, doing this means the call sites must change: you will need
    // special handling for this BRDF compared to other BRDFs. There is no discernible performance
    // improvement by using the simpler estimator.
    float denom = 4.0f * dot(V, H);
    pdf         = computeGGXSmithPDF(V, H, alpha) / denom;

    // Return black if the view and light directions are not in the same hemisphere.
    if (!sameHemisphere(V, L))
    {
        return BLACK;
    }

    // Evaluate the GGX-Smith BRDF with view direction and sampled light direction.
    float3 F; // output not used
    return evaluateGGXSmithBRDF(V, L, f0, roughness, anisotropy, F);
}

#endif // __BSDF_H__