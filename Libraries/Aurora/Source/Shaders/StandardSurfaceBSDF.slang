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
#ifndef __STANDARD_SURFACE_H__
#define __STANDARD_SURFACE_H__

#include "BSDFCommon.slang"

// NOTE: The following shader code is ported to HLSL from the MaterialX GLSL implementations.

// Determines whether vector a and b are in the same hemisphere relative to normal n.
bool sameHemisphere(float3 a, float3 b, float3 n)
{
    return dot(a, n) * dot(b, n) > 0.0f;
}

// Compute the 2D anisotropic roughness in tangent space.
// See
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// NOTE: From libraries/pbrlib/genglsl/mx_roughness_anisotropy.glsl in the MaterialX repo.
float2 mx_roughness_anisotropy(float roughness, float anisotropy)
{
    float2 result = 0.0f.xx;

    // Compute the clamped roughness squared.
    float roughness_sqr = clamp(roughness * roughness, M_FLOAT_EPS, 1.0);

    // If anisotropy is non-zero then compute anisotropic roughness.
    if (anisotropy > 0.0)
    {
        // Convert anisotropy to aspect ratio between 0 and 10.0.
        float aspect = sqrt(1.0 - clamp(anisotropy, 0.0, 0.98));

        // Compute 2D anisotropic roughness from aspect ratio.
        result.x = min(roughness_sqr / aspect, 1.0);
        result.y = roughness_sqr * aspect;
    }
    else
    {
        // If not, just return roughness squared.
        result.x = roughness_sqr;
        result.y = roughness_sqr;
    }

    return result;
}

float mx_square(float x)
{
    return x * x;
}

float mx_pow5(float x)
{
    return mx_square(mx_square(x)) * x;
}

// Mix two BSDFs based on weight
// NOTE: Based on mix functions in mx_mix_bsdf_transmission from
// libraries/pbrlib/genglsl/mx_mix_brdf.glsl in the MaterialX repo.
float3 mx_mix(BSDF bg, BSDF fg, float w)
{
    return lerp(bg, fg, clamp(w, 0.0f, 1.0f));
}

float mx_orennayar(float3 L, float3 V, float3 N, float NdotL, float roughness)
{
    float LdotV = dot(L, V);
    float NdotV = abs(dot(N, V));

    float t = LdotV - NdotL * NdotV;
    t       = t > 0.0 ? t / max(NdotL, NdotV) : 0.0;

    float sigma2 = mx_square(roughness * M_PI);
    float A      = 1.0 - 0.5 * (sigma2 / (sigma2 + 0.33));
    float B      = 0.45f * sigma2 / (sigma2 + 0.09);

    return A + B * t;
}

BSDF mx_diffuse_brdf_reflection(
    float3 L, float3 V, float weight, float3 color, float roughness, float3 normal)
{
    // There is no contribution if the weight is close to zero, or if the light and view directions
    // are not in the same hemisphere relative to the normal.
    if (weight < M_FLOAT_EPS || !sameHemisphere(L, V, normal))
    {
        return BLACK;
    }

    BSDF result = 0.0f.xxx;

    float NdotL = max(dot(L, normal), 0.0);

    result = color * weight * NdotL * M_PI_INV;
    if (roughness > 0.0)
    {
        result *= mx_orennayar(L, V, normal, NdotL, roughness);
    }

    return result;
}

// Compute the average of an anisotropic roughness pair
float mx_average_roughness(float2 roughness)
{
    return sqrt(roughness.x * roughness.y);
}

// Convert a real-valued index of refraction to normal-incidence reflectivity.
float mx_ior_to_f0(float ior)
{
    return mx_square((ior - 1.0) / (ior + 1.0));
}

// https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
// Appendix B.2 Equation 13
float mx_ggx_NDF(float3 X, float3 Y, float3 H, float NdotH, float alphaX, float alphaY)
{
    float XdotH = dot(X, H);
    float YdotH = dot(Y, H);
    float denom = mx_square(XdotH / alphaX) + mx_square(YdotH / alphaY) + mx_square(NdotH);
    return 1.0 / (M_PI * alphaX * alphaY * mx_square(denom));
}

// Standard Schlick Fresnel
float mx_fresnel_schlick(float cosTheta, float F0)
{
    float x  = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x5 = mx_pow5(x);
    return F0 + (1.0 - F0) * x5;
}

// http://jcgt.org/published/0003/02/03/paper.pdf
// Equations 72 and 99
float mx_ggx_smith_G(float NdotL, float NdotV, float alpha)
{
    float alpha2  = mx_square(alpha);
    float lambdaL = sqrt(alpha2 + (1.0 - alpha2) * mx_square(NdotL));
    float lambdaV = sqrt(alpha2 + (1.0 - alpha2) * mx_square(NdotV));
    return 2.0 / (lambdaL / NdotL + lambdaV / NdotV);
}

// NOTE: The MaterialX implementation offers three options for computing directional albedo. Only
// the curve fit (below) is supported here.

// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
float3 mx_ggx_directional_albedo_curve_fit(float NdotV, float roughness, float3 F0, float3 F90)
{
    const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
    float4 r        = roughness * c0 + c1;
    float a004      = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    float2 AB       = float2(-1.04, 1.04) * a004 + r.zw;
    return F0 * AB.x + F90 * AB.y;
}

float mx_ggx_directional_albedo(float NdotV, float roughness, float F0, float F90)
{
    return mx_ggx_directional_albedo_curve_fit(NdotV, roughness, F0.xxx, F90.xxx).x;
}

// https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf
// Equations 14 and 16
float3 mx_ggx_energy_compensation(float NdotV, float roughness, float3 Fss)
{
    float Ess = mx_ggx_directional_albedo(NdotV, roughness, 1.0, 1.0);
    return 1.0 + Fss * (1.0 - Ess) / Ess;
}

float mx_ggx_energy_compensation(float NdotV, float roughness, float Fss)
{
    return mx_ggx_energy_compensation(NdotV, roughness, Fss.xxx).x;
}

BSDF mx_dielectric_brdf_reflection(float3 L, float3 V, float weight, float3 tint, float ior,
    float2 roughness, float3 N, float3 X, int distribution, BSDF base)
{
    BSDF result = 0.0f.xxx;

    if (weight < M_FLOAT_EPS)
    {
        return base;
    }

    float NdotL = abs(dot(N, L));
    float NdotV = abs(dot(N, V));

    float3 Y = normalize(cross(N, X));

    float3 H    = normalize(L + V);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);

    float avgRoughness = mx_average_roughness(roughness);
    float F0           = mx_ior_to_f0(ior);

    float D = mx_ggx_NDF(X, Y, H, NdotH, roughness.x, roughness.y);
    float F = mx_fresnel_schlick(VdotH, F0);
    float G = mx_ggx_smith_G(NdotL, NdotV, avgRoughness);

    float comp      = mx_ggx_energy_compensation(NdotV, avgRoughness, F);
    float dirAlbedo = mx_ggx_directional_albedo(NdotV, avgRoughness, F0, 1.0) * comp;

    // Note: NdotL is canceled out
    result = abs(D * F * G * comp * tint * weight / (4 * NdotV)); // Top layer reflection
    result += base *(1.0 - dirAlbedo * weight); // Base layer reflection attenuated by top layer

    return result;
}

void mx_artistic_to_complex_ior(
    float3 reflectivity, float3 edge_color, out float3 ior, out float3 extinction)
{
    float3 r      = clamp(reflectivity, 0.0, 0.99);
    float3 r_sqrt = sqrt(r);
    float3 n_min  = (1.0 - r) / (1.0 + r);
    float3 n_max  = (1.0 + r_sqrt) / (1.0 - r_sqrt);
    ior           = lerp(n_max, n_min, edge_color);

    float3 np1 = ior + 1.0;
    float3 nm1 = ior - 1.0;
    float3 k2  = (np1 * np1 * r - nm1 * nm1) / (1.0 - r);
    k2         = max(k2, 0.0);
    extinction = sqrt(k2);
}

float3 mx_fresnel_conductor(float cosTheta, float3 n, float3 k)
{
    float c2     = cosTheta * cosTheta;
    float3 n2_k2 = n * n + k * k;
    float3 nc2   = 2.0 * n * cosTheta;

    float3 rs_a = n2_k2 + c2;
    float3 rp_a = n2_k2 * c2 + 1.0;
    float3 rs   = (rs_a - nc2) / (rs_a + nc2);
    float3 rp   = (rp_a - nc2) / (rp_a + nc2);

    return 0.5 * (rs + rp);
}

BSDF mx_conductor_brdf_reflection(float3 L, float3 V, float weight, float3 reflectivity,
    float3 edge_color, float2 roughness, float3 N, float3 X, int distribution)
{
    BSDF result = 0.0f.xxx;

    if (weight < M_FLOAT_EPS || !sameHemisphere(L, V, N))
    {
        return result;
    }

    float NdotL = abs(dot(N, L));
    float NdotV = abs(dot(N, V));

    float3 Y = normalize(cross(N, X));

    float3 H    = normalize(L + V);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);

    float3 ior_n, ior_k;
    mx_artistic_to_complex_ior(reflectivity, edge_color, ior_n, ior_k);

    float avgRoughness = mx_average_roughness(roughness);

    float D  = mx_ggx_NDF(X, Y, H, NdotH, roughness.x, roughness.y);
    float3 F = mx_fresnel_conductor(VdotH, ior_n, ior_k);
    float G  = mx_ggx_smith_G(NdotL, NdotV, avgRoughness);

    float3 comp = mx_ggx_energy_compensation(NdotV, avgRoughness, F);

    result = D * F * G * comp * weight / (4 * NdotV);

    return result;
}

// Computes the normal distribution function used in sheen BRDF.
// Based on:
// http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
// Equation 2
// NOTE: From libraries/pbrlib/genglsl/mx_sheen_brdf.glsl in the MaterialX repo.
float mx_imageworks_sheen_NDF(float cosTheta, float roughness)
{
    // Given roughness is assumed to be clamped to [M_FLOAT_EPS, 1.0]
    float invRoughness = 1.0 / roughness;
    float cos2         = cosTheta * cosTheta;
    float sin2         = 1.0 - cos2;
    return (2.0 + invRoughness) * pow(sin2, invRoughness * 0.5) / (2.0 * M_PI);
}

// LUT for sheen directional albedo.
// Pre-calculated using analytical integration as described in:
// https://www.researchgate.net/profile/Peter_Shirley/publication/220721563_A_microfacet-based_BRDF_generator/links/0046351ded39154fa9000000/A-microfacet-based-BRDF-generator.pdf
// A 2D table parameterized with 'cosTheta' (cosine of angle to normal) on x-axis and 'roughness' on
// y-axis.
// NOTE: From libraries/pbrlib/genglsl/mx_sheen_brdf.glsl in the MaterialX repo.
#define SHEEN_ALBEDO_TABLE_SIZE 16
static const float u_sheenAlbedo[SHEEN_ALBEDO_TABLE_SIZE * SHEEN_ALBEDO_TABLE_SIZE] = { 1.6177,
    0.978927, 0.618938, 0.391714, 0.245177, 0.150234, 0.0893475, 0.0511377, 0.0280191, 0.0144204,
    0.00687674, 0.00295935, 0.00111049, 0.000336768, 7.07119e-05, 6.22646e-06, 1.1084, 0.813928,
    0.621389, 0.479304, 0.370299, 0.284835, 0.21724, 0.163558, 0.121254, 0.0878921, 0.0619052,
    0.0419894, 0.0270556, 0.0161443, 0.00848212, 0.00342323, 0.930468, 0.725652, 0.586532, 0.479542,
    0.393596, 0.322736, 0.26353, 0.213565, 0.171456, 0.135718, 0.105481, 0.0800472, 0.0588117,
    0.0412172, 0.0268329, 0.0152799, 0.833791, 0.671201, 0.558957, 0.471006, 0.398823, 0.337883,
    0.285615, 0.240206, 0.200696, 0.16597, 0.135422, 0.10859, 0.0850611, 0.0644477, 0.0464763,
    0.0308878, 0.771692, 0.633819, 0.537877, 0.461939, 0.398865, 0.344892, 0.297895, 0.256371,
    0.219562, 0.186548, 0.156842, 0.130095, 0.10598, 0.0841919, 0.0645311, 0.04679, 0.727979,
    0.606373, 0.52141, 0.453769, 0.397174, 0.348337, 0.305403, 0.267056, 0.232655, 0.201398,
    0.17286, 0.146756, 0.122808, 0.100751, 0.0804254, 0.0616485, 0.695353, 0.585281, 0.508227,
    0.44667, 0.394925, 0.350027, 0.310302, 0.274561, 0.242236, 0.212604, 0.185281, 0.16002, 0.13657,
    0.114693, 0.0942543, 0.0750799, 0.669981, 0.568519, 0.497442, 0.440542, 0.392567, 0.350786,
    0.313656, 0.280075, 0.249533, 0.221359, 0.195196, 0.170824, 0.148012, 0.126537, 0.106279,
    0.0870713, 0.649644, 0.554855, 0.488453, 0.435237, 0.390279, 0.351028, 0.316036, 0.284274,
    0.255266, 0.228387, 0.203297, 0.179796, 0.157665, 0.136695, 0.116774, 0.0977403, 0.632951,
    0.543489, 0.480849, 0.430619, 0.388132, 0.350974, 0.317777, 0.287562, 0.259885, 0.234153,
    0.210041, 0.187365, 0.165914, 0.145488, 0.125983, 0.10724, 0.61899, 0.533877, 0.47433, 0.426573,
    0.386145, 0.35075, 0.319078, 0.290197, 0.263681, 0.238971, 0.215746, 0.193838, 0.173043,
    0.153167, 0.134113, 0.115722, 0.607131, 0.52564, 0.468678, 0.423001, 0.38432, 0.35043, 0.320072,
    0.292349, 0.266856, 0.243055, 0.220636, 0.199438, 0.179264, 0.159926, 0.141332, 0.123323,
    0.596927, 0.518497, 0.463731, 0.419829, 0.382647, 0.350056, 0.320842, 0.294137, 0.269549,
    0.246564, 0.224875, 0.204331, 0.18474, 0.165919, 0.147778, 0.130162, 0.588052, 0.512241,
    0.459365, 0.416996, 0.381114, 0.349657, 0.321448, 0.295641, 0.271862, 0.24961, 0.228584,
    0.208643, 0.189596, 0.171266, 0.153566, 0.136341, 0.580257, 0.506717, 0.455481, 0.41445,
    0.379708, 0.34925, 0.321929, 0.296923, 0.273869, 0.252279, 0.231859, 0.212472, 0.193933,
    0.176066, 0.158788, 0.141945, 0.573355, 0.5018, 0.452005, 0.412151, 0.378416, 0.348844,
    0.322316, 0.298028, 0.275627, 0.254638, 0.234772, 0.215896, 0.197828, 0.180398, 0.163522,
    0.147049 };

// Looks up an albedo using the static LUT.
// NOTE: From libraries/pbrlib/genglsl/mx_sheen_brdf.glsl in the MaterialX repo.
float mx_imageworks_sheen_directional_albedo(float cosTheta, float roughness)
{
    // Compute x and y index within LUT from roughness and cosTheta.
    float x  = cosTheta * (SHEEN_ALBEDO_TABLE_SIZE - 1);
    float y  = roughness * (SHEEN_ALBEDO_TABLE_SIZE - 1);
    int ix   = int(x);
    int iy   = int(y);
    int ix2  = clamp(ix + 1, 0, SHEEN_ALBEDO_TABLE_SIZE - 1);
    int iy2  = clamp(iy + 1, 0, SHEEN_ALBEDO_TABLE_SIZE - 1);
    float fx = x - ix;
    float fy = y - iy;

    // Bi-linear interpolation of the LUT values
    float v1     = lerp(u_sheenAlbedo[iy * SHEEN_ALBEDO_TABLE_SIZE + ix],
        u_sheenAlbedo[iy * SHEEN_ALBEDO_TABLE_SIZE + ix2], fx);
    float v2     = lerp(u_sheenAlbedo[iy2 * SHEEN_ALBEDO_TABLE_SIZE + ix],
        u_sheenAlbedo[iy2 * SHEEN_ALBEDO_TABLE_SIZE + ix2], fx);
    float albedo = lerp(v1, v2, fy);

    // Return the clamped value.
    return clamp(albedo, 0.0, 1.0);
}

// Compute the reflection component of the sheen BRDF.
// NOTE: From libraries/pbrlib/genglsl/mx_sheen_brdf.glsl in the MaterialX repo.
BSDF mx_sheen_brdf_reflection(
    float3 L, float3 V, float weight, float3 color, float roughness, float3 N, BSDF base)
{
    // Ignore if the weight less than epsilon.
    if (weight < M_FLOAT_EPS)
    {
        return base;
    }

    BSDF result = 0.0f.xxx;

    // Make sure we are forward facing.
    if (dot(N, V) < 0.0)
    {
        N = -N;
    }

    // Compute the half angle.
    float3 H = normalize(L + V);

    // Dot-products of normal with light, view and half-angle.
    float NdotL = clamp(dot(N, L), M_FLOAT_EPS, 1.0);
    float NdotV = clamp(dot(N, V), M_FLOAT_EPS, 1.0);
    float NdotH = clamp(dot(N, H), M_FLOAT_EPS, 1.0);

    // Alpha is clamped roughness (so we can safely take the inverse without worrying about NaNs.)
    float alpha = clamp(roughness, M_FLOAT_EPS, 1.0);

    // Compute microfacet normal distribution function for the surface.
    float D = mx_imageworks_sheen_NDF(NdotH, alpha);

    // Geometry term is skipped and we use a smoother denominator, as in:
    // https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
    float3 fr = D * color / (4.0 * (NdotL + NdotV - NdotL * NdotV));

    // Compute the albedo using LUT.
    float dirAlbedo = mx_imageworks_sheen_directional_albedo(NdotV, alpha);

    // We need to include NdotL from the light integral here
    // as in this case it's not canceled out by the BRDF denominator.
    result = fr * NdotL * weight             // Top layer reflection
        + base * (1.0 - dirAlbedo * weight); // Base layer reflection attenuated by top layer

    return result;
}

// Compute the specular component of the sheen BRDF.
// NOTE: From libraries/pbrlib/genglsl/mx_sheen_brdf.glsl in the MaterialX repo.
BSDF mx_sheen_brdf_specular_layer(
    float3 L, float3 V, float weight, float3 color, float roughness, float3 N)
{
    // There is no contribution if the light and view directions are not in the same hemisphere
    // relative to the normal.
    if (!sameHemisphere(L, V, N))
    {
        return BLACK;
    }

    BSDF result = 0.0f.xxx;

    // Make sure we are forward facing.
    if (dot(N, V) < 0.0)
    {
        N = -N;
    }

    // Compute the half angle.
    float3 H = normalize(L + V);

    // Dot-products of normal with light, view and half-angle.
    float NdotL = clamp(dot(N, L), M_FLOAT_EPS, 1.0);
    float NdotV = clamp(dot(N, V), M_FLOAT_EPS, 1.0);
    float NdotH = clamp(dot(N, H), M_FLOAT_EPS, 1.0);

    // Alpha is clamped roughness (so we can safely take the inverse without worrying about NaNs.)
    float alpha = clamp(roughness, M_FLOAT_EPS, 1.0);

    // Compute microfacet normal distribution function for the surface.
    float D = mx_imageworks_sheen_NDF(NdotH, alpha);

    // Geometry term is skipped and we use a smoother denominator, as in:
    // https://blog.selfshadow.com/publications/s2013-shading-course/rad/s2013_pbs_rad_notes.pdf
    float3 fr = D * color / (4.0 * (NdotL + NdotV - NdotL * NdotV));

    // Compute the albedo using LUT.
    float dirAlbedo = mx_imageworks_sheen_directional_albedo(NdotV, alpha);

    // We need to include NdotL from the light integral here
    // as in this case it's not canceled out by the BRDF denominator.
    result = fr * NdotL * weight;

    return result;
}

// Fake subsurface scattering with simple diffuse transmission.
// NOTE: From /libraries/pbrlib/genglsl/mx_subsurface_brdf.glsl in the MaterialX repo.
BSDF mx_subsurface_brdf_reflection(float3 L, float3 V, float weight, float3 color, float3 normal)
{
    // Invert normal since we're transmitting light from the other side
    float NdotL = dot(L, -normal);
    if (NdotL <= 0.0 || weight < M_FLOAT_EPS)
    {
        return BLACK;
    }

    // Compute basic NdotL from back-facing normal
    return color * weight * NdotL * M_PI_INV;
}

// Create a rotation matrix from axis and angle in radians.
// NOTE: From libraries/stdlib/genglsl/mx_rotate_vector3.glsl in the MaterialX repo.
float4x4 mx_rotationMatrix(float3 axis, float angle)
{
    // Normalize axis.
    axis = normalize(axis);

    // Compute sin and cos of angle.
    float s  = sin(angle);
    float c  = cos(angle);
    float oc = 1.0 - c;

    // Build transform matrix.s
    return float4x4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,
        oc * axis.z * axis.x + axis.y * s, 0.0, oc * axis.x * axis.y + axis.z * s,
        oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0.0,
        oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s,
        oc * axis.z * axis.z + c, 0.0, 0.0, 0.0, 0.0, 1.0);
}

// Rotate a vector around given axis by angle in degrees.
// NOTE: From libraries/stdlib/genglsl/mx_rotate_vector3.glsl in the MaterialX repo.
float3 mx_rotate_vector3(float3 vec, float amount, float3 axis)
{
    float rotationRadians = radians(amount);
    float4x4 m            = mx_rotationMatrix(axis, rotationRadians);
    return mul(m, float4(vec, 1.0)).xyz;
}

// Diffuse component of mx_dielectric_brdf_reflection used for sampling.
// Scales the base diffuse value by the GGX directional albedo.
BSDF mx_dielectric_brdf_diffuse_layer(float3 L, float3 V, float weight, float ior, float2 roughness,
    float3 N, float3 X, int distribution, float3 albedo)
{
    // There is no contribution if the weight is close to zero, or if the light and view directions
    // are not in the same hemisphere relative to the normal.
    if (weight < M_FLOAT_EPS || !sameHemisphere(L, V, N))
    {
        return BLACK;
    }

    // Normal dot products
    float NdotL = abs(dot(N, L));
    float NdotV = abs(dot(N, V));

    // Half-vector dot products
    float3 H    = normalize(L + V);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);

    // Compute the Fresnel term
    float avgRoughness = mx_average_roughness(roughness);
    float F0           = mx_ior_to_f0(ior);
    float F            = mx_fresnel_schlick(VdotH, F0);

    // Compute the energy compensation term (compensates for scattering between micro-surfaces)
    float comp = mx_ggx_energy_compensation(NdotV, avgRoughness, F);
    // Compute the directional albedo term.
    float dirAlbedo = mx_ggx_directional_albedo(NdotV, avgRoughness, F0, 1.0) * comp;

    // The diffuse term is one minus the directional albedo.
    float3 result = 1.0 - dirAlbedo;

    // Return scaled result.
    return result * albedo * weight * M_PI_INV;
}

// Specular component of mx_dielectric_brdf_reflection used for sampling.
// NOTE: We may want to replace this with sampleGGXSmithBRDF() to match the sampleGGXSmithVNDF()
// already being used for sampling.
BSDF mx_dielectric_brdf_specular_layer(float3 L, float3 V, float3 tint, float ior, float2 roughness,
    float3 N, float3 X, int distribution)
{
    // There is no contribution if the light and view directions are not in the same hemisphere
    // relative to the normal.
    if (!sameHemisphere(L, V, N))
    {
        return BLACK;
    }

    BSDF result = 0.0f.xxx;

    // Normal dot products
    float NdotL = abs(dot(N, L));
    float NdotV = abs(dot(N, V));

    // Compute bitangent
    float3 Y = normalize(cross(N, X));

    // Half-vector dot products
    float3 H    = normalize(L + V);
    float NdotH = dot(N, H);
    float VdotH = dot(V, H);

    // Compute average roughness.
    float avgRoughness = mx_average_roughness(roughness);
    float F0           = mx_ior_to_f0(ior);

    // Compute distribtion, fresnel, and geometry terms.
    float D = mx_ggx_NDF(X, Y, H, NdotH, roughness.x, roughness.y);
    float F = mx_fresnel_schlick(VdotH, F0);
    float G = mx_ggx_smith_G(NdotL, NdotV, avgRoughness);

    // Compute the energy compensation term (compensates for scattering between micro-surfaces)
    float comp = mx_ggx_energy_compensation(NdotV, avgRoughness, F);

    // Combine into final specular term.
    // Note: NdotL is canceled out
    return abs(D * F * G * comp * tint / (4 * NdotV * NdotL));
}

// Various flags to enable or disable material operations.
#define ENABLE_SUBSURFACE 1   // subsurface lobe
#define ENABLE_SHEEN 1        // sheen lobe
#define ENABLE_COAT 1         // coat lobe
#define ENABLE_TRANSMISSION 1 // transmission lobe

// Debug flag, to enable simple hemisphere sampling.
#define SIMPLE_HEMISPHERE_SAMPLING 0

// Evaluates the material BSDF with the specified view (toward the viewer) and light (toward the
// light) directions.
// NOTE: This includes the cosine term from the rendering equation.
BSDF_AND_COSINE evaluateMaterial(
    Material material, ShadingData shading, float3 Vw, float3 Lw)
{
    // Roughness influenced by coat.
    float coat_affect_roughness_factor =
        material.coatAffectRoughness * material.coat * material.coatRoughness;
    float coat_affected_roughness =
        lerp(material.specularRoughness, 1.0, coat_affect_roughness_factor);

    // Subsurface color influenced by coat.
    float coat_clamped                    = clamp(material.coat, 0, 1);
    float coat_gamma                      = coat_clamped * material.coatAffectColor + 1.0;
    float3 coat_affected_subsurface_color = pow(material.subsurfaceColor, coat_gamma);

    // Apply the subsurface scale to the subsurface radius.
    // NOTE: From node subsurface_radius_scaled in libraries/bxdf/standard_surface.mtlx in the
    // MaterialX repo.
    float3 subsurface_radius_scaled = material.subsurfaceRadius * material.subsurfaceScale;

    float2 main_roughness =
        mx_roughness_anisotropy(coat_affected_roughness, material.specularAnisotropy);

    // Tangent rotation.
    float3 main_tangent =
        mx_rotate_vector3(shading.tangent, material.specularRotation * 360.0, shading.normal);

    // Coat tangent rotation.
    float3 coat_tangent =
        mx_rotate_vector3(shading.tangent, material.coatRotation * 360.0, shading.normal);

    // Colors influenced by coat ("coat gamma").
    float3 coat_affected_diffuse_color = pow(material.baseColor, coat_gamma);

    // Diffuse layer.
    BSDF diffuse_bsdf = mx_diffuse_brdf_reflection(Lw, Vw, material.base,
        coat_affected_diffuse_color, material.diffuseRoughness, shading.normal);

    // Thin film subsurface.
    // TODO

    BSDF subsurface_mix = diffuse_bsdf;

#if ENABLE_SUBSURFACE
    if (material.subsurface > 0.0)
    {
        // Subsurface layer.
        BSDF subsurface_bsdf = mx_subsurface_brdf_reflection(
            Lw, Vw, 1.000000, coat_affected_subsurface_color, shading.normal);

        // Mix the diffuse BSDF and subsurface BSDF
        // NOTE: From node subsurface_mix in libraries/bxdf/standard_surface.mtlx in the MaterialX
        // repo.
        subsurface_mix = mx_mix(diffuse_bsdf, subsurface_bsdf, material.subsurface);
    }
#endif

    // Subsurface is the only lobe that needs back face lighting, so if the view and light
    // directions are not in the same hemisphere, then return here.
    if (dot(shading.normal, Vw) * dot(shading.normal, Lw) < 0.0f)
    {
        return subsurface_mix;
    }

    // Sheen layer, which takes the subsurface and diffuse as an input
    BSDF sheen_and_diffuse = subsurface_mix;
#if ENABLE_SHEEN
    if (material.sheen > 0.0)
    {
        sheen_and_diffuse = mx_sheen_brdf_reflection(Lw, Vw, material.sheen, material.sheenColor,
            material.sheenRoughness, shading.normal, subsurface_mix);
    }
#endif

    // Factor in transmission layer.
    // In the GLSL implementation this does a fake transmission step here.  For pathtracing just
    // scale by 1-transmission.
    sheen_and_diffuse *= 1.0 - material.transmission;

    // Specular layer.
    BSDF specular_bsdf = sheen_and_diffuse;
    if (material.specular > 0.0)
    {
        specular_bsdf = mx_dielectric_brdf_reflection(Lw, Vw, material.specular,
            material.specularColor, material.specularIOR, main_roughness, shading.normal,
            main_tangent, 0, sheen_and_diffuse);
    }

    // Metal layer.
    BSDF metalness_mix = specular_bsdf;
    if (material.metalness > 0.0)
    {
        float3 metal_reflectivity = material.metalColor * material.base;
        float3 metal_edgecolor    = material.specularColor * material.specular;
        BSDF metal_bsdf           = mx_conductor_brdf_reflection(Lw, Vw, 1.0f, metal_reflectivity,
            metal_edgecolor, main_roughness, shading.normal, main_tangent, 0);
        metalness_mix             = mx_mix(specular_bsdf, metal_bsdf, material.metalness);
    }

    // Coat layer.
    BSDF final_color = metalness_mix;
#if ENABLE_COAT
    if (material.coat > 0.0)
    {
        // Compute 2D anisotropic roughness for coat.
        float2 coat_roughness =
            mx_roughness_anisotropy(material.coatRoughness, material.coatAnisotropy);

        // Attenuate the output of the metal layer base.
        float3 coat_attenuation         = lerp(WHITE, material.coatColor, material.coat);
        float3 metalness_mix_attenuated = coat_attenuation * metalness_mix;

        // The coat layer is a second dielectric specular layer, using attenuated output from metal
        // layer as base.
        final_color = mx_dielectric_brdf_reflection(Lw, Vw, material.coat, material.coatColor,
            material.coatIOR, coat_roughness, shading.normal, coat_tangent, 0,
            metalness_mix_attenuated);
    }
#endif

    return final_color;
}

// Sample direction on unit hemisphere and compute PDF.
float3 calculateHemisphereLightDirection(
    float3 tangent, float3 bitangent, float3 normal, float3 V, float2 random, out float pdf)
{
    float3 Nh = float3(0.0f, 0.0f, V.z > 0.0f ? 1.0f : -1.0f);
    float3 L  = sampleHemisphere(random, Nh, pdf);

    return L;
}

// Compute PDF on unit hemisphere.
float calculateHemispherePDF(float3 normal, float3 Lw)
{
    return dot(normal, Lw) * M_PI_INV;
}

// Compute GGX PDF for given direction using VNDF.
float calculateGGXPDF(float3 V, float2 alpha, float2 random, float3 L)
{

    float3 H = normalize(V + L);

    float denom = 4.0f * dot(V, H);
    float pdf   = evaluateGGXSmithVNDF(V, H, alpha) / denom;

    return pdf;
}

// Compute GGX tangent-space light direction from random 2D value, along with half-vector,
// world-space light direction and GGX PDF.
// NOTE: This is an abbreviated version of sampleGGXSmithBRDF().
float3 calculateGGXLightDirectionAndPDF(float3 tangent, float3 bitangent, float3 normal, float3 V,
    float2 alpha, float2 random, out float3 Lw, out float pdf)
{
    // Sample the GGX-Smith VNDF to determine a microfacet normal (half-vector). If the view
    // direction is in the lower hemisphere in tangent space, flip the view direction for sampling
    // the VNDF, as the VNDF expects the view direction to be in the upper hemisphere (with the
    // normal). Then flip the resulting microfacet normal.
    bool flip = V.z <= 0.0f;
    float3 H  = sampleGGXSmithVNDF(flip ? -V : V, alpha, random);
    H         = flip ? -H : H;

    // Reflect the view direction across the microfacet normal to get the sample direction.
    float3 L = reflect(-V, H);

    // Compute the PDF, divided by a factor from using a reflected vector.
    float denom = 4.0f * dot(V, H);
    pdf         = computeGGXSmithPDF(V, H, alpha) / denom;

    // Compute world space light direction.
    Lw = tangentToWorld(L, tangent, bitangent, normal);

    return L;
}

// Samples the material using simple cosine-weighted hemisphere sampling.
// NOTE: This will provide suboptimal results, and is only intended for debugging.
BSDF_AND_COSINE sampleMaterialAllLobes(Material material, ShadingData shading, float3 Vw,
    float2 random, out float3 Lw, out float pdf, out int lobeID)
{
    lobeID = 1;
    // Sample a cosine-weighted direction from a hemisphere above the normal. If the view direction
    // is not in the same hemisphere as the normal, flip the light direction to allow proper shading
    // of the backface.
    // TODO: Proper importance sampling of Standard Surface, similar to the reference material.
    Lw = sampleHemisphere(random, shading.normal, pdf);
    if (dot(shading.normal, Vw) < 0.0f)
    {
        Lw = -Lw;
    }

    // Evaluate the material with the view direction and sampled light direction.
    return evaluateMaterial(material, shading, Vw, Lw);
}

// Samples the material (for a single randomly selected lobe) with the specified view
// direction (toward the viewer) and sampled light direction (toward the light), returning
// the evaluated BSDF, the corresponding PDF, and an integer lobe ID indicating the BSDF lobe
// that was sampled.
// NOTE: This includes the cosine term from the rendering equation.

BSDF_AND_COSINE evaluateMaterialAndPDF(Material material, ShadingData shading,
    float3 Vw, float3 Lw, float2 random, out float pdf, out int lobeID)
{
    // Transform the view direction to tangent space.
    float3 N    = shading.normal;
    float3 X    = shading.tangent;
    float3 Y    = shading.bitangent;
    float3 V    = worldToTangent(Vw, X, Y, N);
    float3 L    = worldToTangent(Lw, X, Y, N);
    float NdotV = abs(V.z);

    // Default values (avoid compile errors if lobes disabled).
    pdf    = 0.0;
    lobeID = -1;

    // Roughness influenced by coat.
    float coat_affect_roughness_factor =
        material.coatAffectRoughness * material.coat * material.coatRoughness;
    float coat_affected_roughness =
        lerp(material.specularRoughness, 1.0, coat_affect_roughness_factor);

    // Subsurface color influenced by coat.
    float coat_clamped                    = clamp(material.coat, 0, 1);
    float coat_gamma                      = coat_clamped * material.coatAffectColor + 1.0;
    float3 coat_affected_subsurface_color = pow(material.subsurfaceColor, coat_gamma);

    // Apply the subsurface scale to the subsurface radius.
    // NOTE: From node subsurface_radius_scaled in libraries/bxdf/standard_surface.mtlx in the
    // MaterialX repo.
    float3 subsurface_radius_scaled = material.subsurfaceRadius * material.subsurfaceScale;

    float2 main_roughness =
        mx_roughness_anisotropy(coat_affected_roughness, material.specularAnisotropy);

    // Tangent rotation.
    float3 main_tangent =
        mx_rotate_vector3(shading.tangent, material.specularRotation * 360.0, shading.normal);

    // Coat tangent rotation.
    float3 coat_tangent =
        mx_rotate_vector3(shading.tangent, material.coatRotation * 360.0, shading.normal);

    // Colors influenced by coat ("coat gamma").
    float3 coat_affected_diffuse_color = pow(material.baseColor, coat_gamma);

    float3 result;

    // Sample coat lobe.
    float coat = ENABLE_COAT ? material.coat : 0.0;

    // Evaluate the specular Fresnel component for coat BSDF.
    float coatF0 = mx_ior_to_f0(material.coatIOR);
    float3 coatF = mx_fresnel_schlick(NdotV, coatF0);

    // Compute luminance specular Fresnel component for coat BSDF.
    float coatLuminance = luminance(coatF * coat * material.coatColor);
    if (random.x < coatLuminance)
    {
#if ENABLE_COAT
        // Remap the random variable to 0-1 within coat lobe.
        random.x = remapPass(random.x, coatLuminance);

        // Compute coat alpha from roughness and aniso.
        float2 coatAlpha = mx_roughness_anisotropy(material.coatRoughness, material.coatAnisotropy);

        // Compte PDF from provided light direction..
        // TODO: Use rotated tangent.
        pdf = calculateGGXPDF(V, coatAlpha, random, L);

        // Compute 2D anisotropic roughness for coat.
        float2 coat_roughness =
            mx_roughness_anisotropy(material.coatRoughness, material.coatAnisotropy);

        // Compute coat color.
        result = mx_dielectric_brdf_specular_layer(Lw, Vw, material.coat * material.coatColor,
            material.coatIOR, coat_roughness, N, coat_tangent, 0);

        // Assign lobe ID.
        lobeID = COAT_LOBE;
#endif
    }
    else
    {
        // Remap the random variable to 0-1 outside coat lobe.
        random.x = remapFail(random.x, coatLuminance);

        // Compute specular alpha from roughness and anisotropy.
        float2 specAlpha =
            mx_roughness_anisotropy(material.specularRoughness, material.specularAnisotropy);

        // Sample metal lobe.
        if (random.x < material.metalness)
        {
            // Remap the random variable to 0-1 within metal lobe.
            random.x = remapPass(random.x, material.metalness);

            // Compte PDF from provided light direction..
            // TODO: Use rotated tangent.
            pdf = calculateGGXPDF(V, specAlpha, random, L);

            // Compute metallic color.
            float3 metal_reflectivity = material.metalColor * material.base;
            float3 metal_edgecolor    = material.specularColor * material.specular;
            result = mx_conductor_brdf_reflection(Lw, Vw, 1.0f, metal_reflectivity, metal_edgecolor,
                main_roughness, N, main_tangent, 0);

            // Remove the N.L term when sampling (which comes from mx_ggx_smith_G), to avoid
            // applying it twice.
            result /= abs(L.z);

            // Set lobe ID.
            lobeID = METAL_LOBE;
            ;
        }
        else
        {
            // Remap the random variable to 0-1 outside metal lobe.
            random.x = remapFail(random.x, material.metalness);

            // Evaluate the specular Fresnel component for BSDF.
            float F0 = mx_ior_to_f0(material.specularIOR);
            float3 F = mx_fresnel_schlick(NdotV, F0);

            // Compute luminance specular Fresnel component for BSDF.
            float specLuminance = luminance(F * material.specular * material.specularColor);

            // Sample glossy lobe.
            if (random.x < specLuminance)
            {
                // Remap the random variable to 0-1 within metal lobe.
                random.x = remapPass(random.x, specLuminance);

                // Compte PDF from provided light direction..
                pdf = calculateGGXPDF(V, specAlpha, random, L);

                // Compute specular color.
                result = mx_dielectric_brdf_specular_layer(Lw, Vw,
                    material.specularColor * material.specular, material.specularIOR,
                    main_roughness, shading.normal, main_tangent, 0);

                // Normalize the result with the probability of having selected this lobe. This is
                // needed for this lobe because the result is inherently scaled by a similar amount
                // above, i.e. F * specular * specularColor. Otherwise the lobe contribution will be
                // too small.
                result /= specLuminance;

                // Set lobe ID.
                lobeID = GLOSSY_LOBE;
            }
            else
            {
                // Remap the random variable to 0-1 within metal lobe.
                random.x = remapFail(random.x, specLuminance);

                // Alpha is just roughness clamped to epsilon-1.0 range (to avoid NaN)
                float sheenAlpha = clamp(material.sheenRoughness, M_FLOAT_EPS, 1.0);

                // Sample sheen node.
                float sheen = ENABLE_SHEEN ? material.sheen : 0.0;
                if (random.x < material.sheen)
                {
#if ENABLE_SHEEN
                    // Remap the random variable to 0-1 within sheen lobe.
                    random.x = remapPass(random.x, sheen);

                    // Compte PDF from provided light direction..
                    pdf = calculateHemispherePDF(N, Lw);

                    // Compute half-vector.
                    float3 H = normalize(L + V);

                    // Compute sheen color.
                    result = mx_sheen_brdf_specular_layer(
                        Lw, Vw, material.sheen, material.sheenColor, material.sheenRoughness, N);

                    // Compute probability density function as sheen normal distribution function.
                    // Uses NDF directly without scaling by cosine values.
                    pdf = mx_imageworks_sheen_NDF(H.z, sheenAlpha);

                    // Set lobe ID.
                    lobeID = SHEEN_LOBE;
#endif
                }
                else
                {
                    // Remap the random variable to 0-1 within diffuse lobe.
                    random.x = remapFail(random.x, sheen);

                    // Compte PDF from provided light direction..
                    pdf = calculateHemispherePDF(N, Lw);

                    // Compute diffuse color.
                    result = mx_dielectric_brdf_diffuse_layer(Lw, Vw, material.base,
                        material.specularIOR, main_roughness, N, main_tangent, 0,
                        coat_affected_diffuse_color);

                    // Apply sheen direction albedo to diffuse color.
                    float dirAlbedo = mx_imageworks_sheen_directional_albedo(NdotV, sheenAlpha);
                    result *= (1.0 -
                        dirAlbedo *
                            material.sheen); // Base layer reflection attenuated by top layer

                    // Set lobe ID.
                    lobeID = DIFFUSE_LOBE;
                }
            }
        }

        // Apply coat attenuation factor to result.
        float3 coat_attenuation = lerp(WHITE, material.coatColor, material.coat);
        result *= coat_attenuation;
    }

    // Multiply the N.L cosine term to final value.
    float NdotL = abs(L.z);

    return result * NdotL;
}

// Samples the material with the specified view direction (toward the viewer), returning the
// evaluated BSDF, the sampled light direction (toward the light) and the corresponding PDF.
// NOTE: This includes the cosine term from the rendering equation.
BSDF_AND_COSINE sampleMaterial(Material material, ShadingData shading, float3 Vw,
    float2 random, out float3 Lw, out float pdf, out int lobeID)
{
    // Transform the view direction to tangent space.
    float3 L;
    float3 N    = shading.normal;
    float3 X    = shading.tangent;
    float3 Y    = shading.bitangent;
    float3 V    = worldToTangent(Vw, X, Y, N);
    float NdotV = abs(V.z);

    // If debug flag is set, just sample all the lobes over hemisphere.
#if SIMPLE_HEMISPHERE_SAMPLING
    BSDF_AND_COSINE res = sampleMaterialAllLobes(material, shading, Vw, random, Lw, pdf, lobeID);
    return res;
#endif

    // Default values (avoid compile errors if lobes disabled).
    Lw     = float3(1, 0, 0);
    pdf    = 0.0;
    lobeID = -1;

    // Roughness influenced by coat.
    float coat_affect_roughness_factor =
        material.coatAffectRoughness * material.coat * material.coatRoughness;
    float coat_affected_roughness =
        lerp(material.specularRoughness, 1.0, coat_affect_roughness_factor);

    // Subsurface color influenced by coat.
    float coat_clamped                    = clamp(material.coat, 0, 1);
    float coat_gamma                      = coat_clamped * material.coatAffectColor + 1.0;
    float3 coat_affected_subsurface_color = pow(material.subsurfaceColor, coat_gamma);

    // Apply the subsurface scale to the subsurface radius.
    // NOTE: From node subsurface_radius_scaled in libraries/bxdf/standard_surface.mtlx in the
    // MaterialX repo.
    float3 subsurface_radius_scaled = material.subsurfaceRadius * material.subsurfaceScale;

    float2 main_roughness =
        mx_roughness_anisotropy(coat_affected_roughness, material.specularAnisotropy);

    // Tangent rotation.
    float3 main_tangent =
        mx_rotate_vector3(shading.tangent, material.specularRotation * 360.0, shading.normal);

    // Coat tangent rotation.
    float3 coat_tangent =
        mx_rotate_vector3(shading.tangent, material.coatRotation * 360.0, shading.normal);

    // Colors influenced by coat ("coat gamma").
    float3 coat_affected_diffuse_color = pow(material.baseColor, coat_gamma);

    float3 result;

    // Sample coat lobe.
    float coat = ENABLE_COAT ? material.coat : 0.0;

    // Evaluate the specular Fresnel component for coat BSDF.
    float coatF0 = mx_ior_to_f0(material.coatIOR);
    float3 coatF = mx_fresnel_schlick(NdotV, coatF0);

    // Compute luminance specular Fresnel component for coat BSDF.
    float coatLuminance = luminance(coatF * coat * material.coatColor);
    if (random.x < coatLuminance)
    {
#if ENABLE_COAT
        // Remap the random variable to 0-1 within coat lobe.
        random.x = remapPass(random.x, coatLuminance);

        // Compute coat alpha from roughness and aniso.
        float2 coatAlpha = mx_roughness_anisotropy(material.coatRoughness, material.coatAnisotropy);

        // Generate light direction and half vector.
        // TODO: Use rotated tangent.
        L = calculateGGXLightDirectionAndPDF(X, Y, N, V, coatAlpha, random, Lw, pdf);

        // Compute 2D anisotropic roughness for coat.
        float2 coat_roughness =
            mx_roughness_anisotropy(material.coatRoughness, material.coatAnisotropy);

        // Compute coat color.
        result = mx_dielectric_brdf_specular_layer(Lw, Vw, coat * material.coatColor,
            material.coatIOR, coat_roughness, N, coat_tangent, 0);

        // Normalize the result with the probability of having selected this lobe. This is
        // needed for this lobe because the result is inherently scaled by a similar amount
        // above, i.e. F * coat * coatColor. Otherwise the lobe contribution will be
        // too small.
        result /= coatLuminance;

        // Assign lobe ID.
        lobeID = COAT_LOBE;
#endif
    }
    else
    {
        // Remap the random variable to 0-1 outside coat lobe.
        random.x = remapFail(random.x, coatLuminance);

        // Compute specular alpha from roughness and anisotropy.
        float2 specAlpha =
            mx_roughness_anisotropy(material.specularRoughness, material.specularAnisotropy);

        // Sample metal lobe.
        if (random.x < material.metalness)
        {
            // Remap the random variable to 0-1 within metal lobe.
            random.x = remapPass(random.x, material.metalness);

            // Generate light direction and half vector.
            // TODO: Use rotated tangent.
            L = calculateGGXLightDirectionAndPDF(X, Y, N, V, specAlpha, random, Lw, pdf);

            // Compute metallic color.
            float3 metal_reflectivity = material.metalColor * material.base;
            float3 metal_edgecolor    = material.specularColor * material.specular;
            result = mx_conductor_brdf_reflection(Lw, Vw, 1.0f, metal_reflectivity, metal_edgecolor,
                main_roughness, N, main_tangent, 0);

            // Remove the N.L term when sampling (which comes from mx_ggx_smith_G), to avoid
            // applying it twice.
            result /= abs(L.z);

            // Set lobe ID.
            lobeID = METAL_LOBE;
        }
        else
        {
            // Remap the random variable to 0-1 outside metal lobe.
            random.x = remapFail(random.x, material.metalness);

            // Evaluate the specular Fresnel component for BSDF.
            float F0 = mx_ior_to_f0(material.specularIOR);
            float3 F = mx_fresnel_schlick(NdotV, F0);

            // Compute luminance specular Fresnel component for BSDF.
            float specLuminance = luminance(F * material.specular * material.specularColor);

            // Sample glossy lobe.
            if (random.x < specLuminance)
            {
                // Remap the random variable to 0-1 within metal lobe.
                random.x = remapPass(random.x, specLuminance);

                // Generate light direction and half vector.
                L = calculateGGXLightDirectionAndPDF(X, Y, N, V, specAlpha, random, Lw, pdf);

                // Compute specular color.
                result = mx_dielectric_brdf_specular_layer(Lw, Vw,
                    material.specularColor * material.specular, material.specularIOR,
                    main_roughness, shading.normal, main_tangent, 0);

                // Normalize the result with the probability of having selected this lobe. This is
                // needed for this lobe because the result is inherently scaled by a similar amount
                // above, i.e. F * specular * specularColor. Otherwise the lobe contribution will be
                // too small.
                result /= specLuminance;

                // Set lobe ID.
                lobeID = GLOSSY_LOBE;
            }
            else
            {
                // Remap the random variable to 0-1 within metal lobe.
                random.x = remapFail(random.x, specLuminance);

                // Alpha is just roughness clamped to epsilon-1.0 range (to avoid NaN)
                float sheenAlpha = clamp(material.sheenRoughness, M_FLOAT_EPS, 1.0);

                // Sample sheen node.
                float sheen = ENABLE_COAT ? material.sheen : 0.0;
                if (random.x < material.sheen)
                {
#if ENABLE_SHEEN
                    // Remap the random variable to 0-1 within sheen lobe.
                    random.x = remapPass(random.x, sheen);

                    // Compute light direction within hemisphere.
                    L  = calculateHemisphereLightDirection(X, Y, N, V, random, pdf);
                    Lw = tangentToWorld(L, X, Y, N);

                    // Compute half-vector.
                    float3 H = normalize(L + V);

                    // Compute sheen color.
                    result = mx_sheen_brdf_specular_layer(
                        Lw, Vw, material.sheen, material.sheenColor, material.sheenRoughness, N);

                    // Compute probability density function as sheen normal distribution function.
                    // Uses NDF directly without scaling by cosine values.
                    pdf = mx_imageworks_sheen_NDF(H.z, sheenAlpha);

                    // Set lobe ID.
                    lobeID = SHEEN_LOBE;
#endif
                }
                else
                {
                    random.x           = remapFail(random.x, sheen);
                    float transmission = ENABLE_TRANSMISSION ? material.transmission : 0.0;
                    if (random.x < transmission)
                    {
#if ENABLE_TRANSMISSION
                        // Remap the random variable (pass case).
                        random.x = remapPass(random.x, transmission);

                        // Set the sampled light direction to the opposite of the view direction,
                        // which is appropriate for thin-walled materials. If this is not a
                        // thin-walled material, compute a refraction direction, or a reflection
                        // direction if total internal reflection occurs.
                        // NOTE: A ternary operator should not be used here as HLSL evaluates both
                        // results, and computeRefraction() has side-effects.
                        bool reflected = false;
                        L              = -V;
                        if (!material.thinWalled)
                        {
                            L = computeRefraction(V, material.specularIOR, reflected);
                        }
                        Lw = tangentToWorld(L, X, Y, N);

                        // The BSDF is simply the transmission color. Since this is a delta
                        // distribution (specular), it must be divided by the cosine term, and the
                        // PDF is simply 1.0.
                        // NOTE: See PBRT 8.2.2 for information on delta distributions.
                        result = material.transmissionColor;
                        result /= abs(L.z);
                        pdf = 1.0f;

                        // Set the lobe ID.
                        // NOTE: Total internal reflection is not actually glossy, but a more
                        // specific state is not required here.
                        lobeID = reflected ? GLOSSY_LOBE : TRANSMISSION_LOBE;
#endif
                    }
                    else
                    {
                        // Remap the random variable to 0-1 outside transmission.
                        random.x = remapFail(random.x, transmission);

                        // Compute light direction within hemisphere.
                        L  = calculateHemisphereLightDirection(X, Y, N, V, random, pdf);
                        Lw = tangentToWorld(L, X, Y, N);

                        // Compute diffuse color.
                        result = mx_dielectric_brdf_diffuse_layer(Lw, Vw, material.base,
                            material.specularIOR, main_roughness, N, main_tangent, 0,
                            coat_affected_diffuse_color);

                        // Apply sheen direction albedo to diffuse color.
                        float dirAlbedo = mx_imageworks_sheen_directional_albedo(NdotV, sheenAlpha);
                        result *= (1.0 -
                            dirAlbedo *
                                material.sheen); // Base layer reflection attenuated by top layer

                        // Set lobe ID.
                        lobeID = DIFFUSE_LOBE;
                    }
                }
            }
        }

        // Apply coat attenuation factor to result.
        float3 coat_attenuation = lerp(WHITE, material.coatColor, material.coat);
        result *= coat_attenuation;
    }

    // Multiply the N.L cosine term to final value.
    float NdotL = abs(L.z);

    return result * NdotL;
}

#endif // __STANDARD_SURFACE_H__
