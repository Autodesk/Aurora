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
#ifndef __REFERENCE_MATERIAL_H__
#define __REFERENCE_MATERIAL_H__

// NOTE: This implements a reference material based on the glTF PBR material, with some
// modifications. See Appendix B here:
// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md

// Evaluates the material BSDF with the specified view (toward the viewer) and light (toward the
// light) directions.
// NOTE: This includes the cosine term from the rendering equation.
BSDF_AND_COSINE evaluateMaterial(
    Material material, ShadingData shading, float3 Vw, float3 Lw)
{
    // Transform the view and light directions to tangent space.
    float3 N = shading.normal;
    float3 X = shading.tangent;
    float3 Y = shading.bitangent;
    float3 V = worldToTangent(Vw, X, Y, N);
    float3 L = worldToTangent(Lw, X, Y, N);

    // If the view and light directions are not in the same hemisphere, then there is no reflected
    // radiance for an opaque material.
    // NOTE: Vectors in opposite hemispheres should result in evaluating transmission (BTDF).
    if (!sameHemisphere(V, L))
    {
        return BLACK;
    }

    // Collect relevant material properties.
    float base           = material.base;
    float3 diffuseColor  = material.baseColor;
    float metalness      = material.metalness;
    float3 metalColor    = material.metalColor;
    float specular       = material.specular;
    float3 specularColor = material.specularColor;
    float roughness      = material.specularRoughness;
    float anisotropy     = material.specularAnisotropy;
    float ior            = material.specularIOR;
    float transmission   = material.transmission;

    // Compute the reflectance at normal incidence (f0). This a mix of dielctric and metal
    // reflectance, based on the metalness.
    float3 f0Dielectric = metalness < 1.0f ? iorToF0(ior) : 0.0f;
    float3 f0Conductor  = metalness > 0.0f ? metalColor : 0.0f;
    float3 f0           = lerp(f0Dielectric, f0Conductor, metalness);

    // Compute the glossy BRDF. For dielectric (non-metal) materials, it is modulated by the
    // specular factor and color; those are not used for metal materials.
    float3 F;
    float3 glossyBRDF = evaluateGGXSmithBRDF(V, L, f0, roughness, anisotropy, F);
    glossyBRDF *= lerp(specular * specularColor, 1.0f, metalness);

    // Compute the diffuse BRDF, and modulate it by the base factor, metalness, and transmission.
    float3 diffuseBRDF = evaluateLambertianBRDF(diffuseColor);
    diffuseBRDF        = base * diffuseBRDF * (1.0f - metalness) * (1.0f - transmission);

    // Sum the glossy and diffuse components, with the latter blended by the Fresnel term. This is
    // multipled by the cosine term (NdotL or L.z) from the rendering equation.
    return (glossyBRDF + diffuseBRDF * (1.0f - F)) * abs(L.z);
}

// Samples the material (for a single randomly selected lobe) with the specified view
// direction (toward the viewer) and sampled light direction (toward the light), returning
// the evaluated BSDF, the corresponding PDF, and an integer lobe ID indicating the BSDF lobe
// that was sampled.
// NOTE: This includes the cosine term from the rendering equation.
BSDF_AND_COSINE evaluateMaterialAndPDF(Material material, ShadingData shading, float3 Vw,
    float3 Lw, float2 random, out float pdf, out int lobeID)
{
    // Transform the view direction to tangent space.
    float3 N = shading.normal;
    float3 X = shading.tangent;
    float3 Y = shading.bitangent;
    float3 V = worldToTangent(Vw, X, Y, N);
    float3 L = worldToTangent(Lw, X, Y, N);
    float3 H = normalize(V + L);

    // If the view and light directions are not in the same hemisphere, then there is no reflected
    // radiance for an opaque material.
    // NOTE: Vectors in opposite hemispheres should result in evaluating transmission (BTDF).
    if (!sameHemisphere(V, L))
    {
        return BLACK;
    }

    // Collect relevant material properties.
    float base           = material.base;
    float3 diffuseColor  = material.baseColor;
    float metalness      = material.metalness;
    float3 metalColor    = material.metalColor;
    float specular       = material.specular;
    float3 specularColor = material.specularColor;
    float roughness      = material.specularRoughness;
    float anisotropy     = material.specularAnisotropy;
    float ior            = material.specularIOR;
    float transmission   = material.transmission;
    bool thinWalled      = material.thinWalled;

    // NOTE: The rest of this function selects a single lobe based on the material properties, using
    // a random variable. That lobe is used to determine a sample direction and is also evaluated.
    // The material lobes are based on Standard Surface and are as follows:
    //
    // - METAL- | ------- GLOSSY -------
    //          | TRANSMISSION | DIFFUSE
    //
    // "P" below is a probability of a lobe being selected in isolation. In general, a lobe should
    // have its contribution divided by the (accumulated) probability of having selected that lobe,
    // but instead each contribution omits that same amount that would normally be applied as a
    // scaling factor, e.g. the diffuse lobe isn't scaled by (1.0 - transmission) here as it is in
    // the evaluation function.

    // METAL: Determine whether this lobe should be evaluated, using the metalness.
    BSDF_AND_COSINE result = 0.0f;
    float P                = metalness;
    if (random.x < P)
    {
        // Remap the random variable (pass case).
        random.x = remapPass(random.x, P);

        // Convert roughness and anisotropy to alpha values.
        float2 alpha = roughnessToAlpha(roughness, anisotropy);

        // Evaluate the GGX-Smith BRDF with view direction and sampled light direction.
        float3 F; // output not used
        result = evaluateGGXSmithBRDF(V, L, metalColor, roughness, anisotropy, F);

        // Compute the PDF, divided by a factor from using a reflected vector.
        // NOTE: See sampleGGXSmithBRDF() for details.
        float denom = 4.0f * dot(V, H);
        pdf         = computeGGXSmithPDF(V, H, alpha) / denom;

        // Set the lobe ID.
        lobeID = METAL_LOBE;
    }

    // GLOSSY (dielectric).
    else
    {
        // Remap the random variable (fail case).
        random.x = remapFail(random.x, P);

        // Compute the Fresnel reflectance term, using the IOR to determine an f0 value. This uses
        // NdotV (V.z) as the cosine term, in the absence of a sampled light direction.
        float3 f0 = iorToF0(ior);
        float3 F  = evaluateSchlickFresnel(f0, abs(V.z));

        // Determine whether this lobe should be evaluated, using the luminance of the Fresnel term
        // along with the specular factor and color.
        P = luminance(F * specular * specularColor);
        if (random.x < P)
        {
            // Remap the random variable (pass case).
            random.x = remapPass(random.x, P);

            // Convert roughness and anisotropy to alpha values.
            float2 alpha = roughnessToAlpha(roughness, anisotropy);

            // Sample and evaluate the GGX-Smith BRDF, for this dielectric glossy lobe. Scale the
            // result with the specular factor and color.
            result = evaluateGGXSmithBRDF(V, L, f0, roughness, anisotropy, F);
            result *= specular * specularColor;

            // Normalize the result with the probability of having selected this lobe. This is
            // needed for this lobe because the result is inherently scaled by a similar amount
            // above, i.e. F * specular * specularColor. Otherwise the lobe contribution will be too
            // small.
            result /= P;

            // Compute the PDF, divided by a factor from using a reflected vector.
            // NOTE: See sampleGGXSmithBRDF() for details.
            float denom = 4.0f * dot(V, H);
            pdf         = computeGGXSmithPDF(V, H, alpha) / denom;

            // Set the lobe ID.
            lobeID = GLOSSY_LOBE;
        }

        // TODO: Fix the typos below.

        // TRANSMISSION (specular).
        // NOTE: This is true specular (impulse) transmission, i.e. sharp refraction, and sharp
        // total internal reflection.
        else
        {
            // Remap the random variable (fail case).
            random.x = remapFail(random.x, P);

            float3 N = float3(0.0f, 0.0f, V.z > 0.0f ? 1.0f : -1.0f);
            pdf      = dot(N, L) * M_PI_INV;

            // Sample and evaluate the Lambertian BRDF, for this dielectric diffuse lobe. Scale
            // the result with the base factor.
            result = evaluateLambertianBRDF(diffuseColor);
            result *= base;

            // Set the lobe ID.
            lobeID = DIFFUSE_LOBE;
        }
    }

    // Return the result multipled by the cosine term (NdotL or L.z) from the rendering equation.
    return result * abs(L.z);
}

// Samples the material with the specified view direction (toward the viewer), returning the
// evaluated BSDF, the sampled light direction (toward the light), the corresponding PDF, and (for
// debug purposes) an integer lobe ID indicating the BSDF lobe that was sampled.
// NOTE: This includes the cosine term from the rendering equation.
BSDF_AND_COSINE sampleMaterial(Material material, ShadingData shading, float3 Vw,
    float2 random, out float3 Lw, out float pdf, out int lobeID)
{
    // Transform the view direction to tangent space.
    float3 N = shading.normal;
    float3 X = shading.tangent;
    float3 Y = shading.bitangent;
    float3 V = worldToTangent(Vw, X, Y, N);

    // Collect relevant material properties.
    float base           = material.base;
    float3 diffuseColor  = material.baseColor;
    float metalness      = material.metalness;
    float3 metalColor    = material.metalColor;
    float specular       = material.specular;
    float3 specularColor = material.specularColor;
    float roughness      = material.specularRoughness;
    float anisotropy     = material.specularAnisotropy;
    float ior            = material.specularIOR;
    float transmission   = material.transmission;
    bool thinWalled      = material.thinWalled;

    // NOTE: The rest of this function selects a single lobe based on the material properties, using
    // a random variable. That lobe is used to determine a sample direction and is also evaluated.
    // The material lobes are based on Standard Surface and are as follows:
    //
    // - METAL- | ------- GLOSSY -------
    //          | TRANSMISSION | DIFFUSE
    //
    // "P" below is a probability of a lobe being selected in isolation. In general, a lobe should
    // have its contribution divided by the (accumulated) probability of having selected that lobe,
    // but instead each contribution omits that same amount that would normally be applied as a
    // scaling factor, e.g. the diffuse lobe isn't scaled by (1.0 - transmission) here, but that is
    // done in the evaluation function.

    // METAL: Determine whether this lobe should be evaluated, using the metalness.
    BSDF_AND_COSINE result = 0.0f;
    float3 L;
    float P = metalness;
    if (random.x < P)
    {
        // Remap the random variable (pass case).
        random.x = remapPass(random.x, P);

        // Sample and evaluate the GGX-Smith BRDF, for this metal lobe.
        result = sampleGGXSmithBRDF(V, metalColor, roughness, anisotropy, random, L, pdf);

        // Set the lobe ID.
        lobeID = METAL_LOBE;
    }

    // GLOSSY (dielectric).
    else
    {
        // Remap the random variable (fail case).
        random.x = remapFail(random.x, P);

        // Compute the Fresnel reflectance term, using the IOR to determine an f0 value. This uses
        // NdotV (V.z) as the cosine term, in the absence of a sampled light direction.
        float3 f0 = iorToF0(ior);
        float3 F  = evaluateSchlickFresnel(f0, abs(V.z));

        // Determine whether this lobe should be evaluated, using the luminance of the Fresnel term
        // along with the specular factor and color.
        P = luminance(F * specular * specularColor);
        if (random.x < P)
        {
            // Remap the random variable (pass case).
            random.x = remapPass(random.x, P);

            // Sample and evaluate the GGX-Smith BRDF, for this dielectric glossy lobe. Scale the
            // result with the specular factor and color.
            result = sampleGGXSmithBRDF(V, f0, roughness, anisotropy, random, L, pdf);
            result *= specular * specularColor;

            // Normalize the result with the probability of having selected this lobe. This is
            // needed for this lobe because the result is inherently scaled by a similar amount
            // above, i.e. F * specular * specularColor. Otherwise the lobe contribution will be too
            // small.
            result /= P;

            // Set the lobe ID.
            lobeID = GLOSSY_LOBE;
        }

        // TRANSMISSION (specular).
        // NOTE: This is true specular (impulse) transmission, i.e. sharp refraction, and sharp
        // total internal reflection.
        else
        {
            // Remap the random variable (fail case).
            random.x = remapFail(random.x, P);

            // Determine whether this lobe should be evaluated, using the transmission factor.
            P = transmission;
            if (random.x < P)
            {
                // Remap the random variable (pass case).
                random.x = remapPass(random.x, P);

                // Set the sampled light direction to the opposite of the view direction, which is
                // appropriate for thin-walled materials. If this is not a thin-walled material,
                // compute a refraction direction, or a reflection direction if total internal
                // reflection occurs.
                // NOTE: A ternary operator should not be used here as HLSL evaluates both results,
                // and computeRefraction() has side-effects.
                bool reflected = false;
                L              = -V;
                if (!material.thinWalled)
                {
                    L = computeRefraction(V, material.specularIOR, reflected);
                }

                // The BSDF is simply the transmission color. Since this is a delta distribution
                // (specular), it must be divided by the cosine term, and the PDF is simply 1.0.
                // NOTE: See PBRT 8.2.2 for information on delta distributions.
                result = material.transmissionColor;
                result /= abs(L.z);
                pdf = 1.0f;

                // Set the lobe ID.
                // NOTE: Total internal reflection is not actually glossy, but a more specific state
                // is not required here.
                lobeID = reflected ? GLOSSY_LOBE : TRANSMISSION_LOBE;
            }

            // DIFFUSE.
            else
            {
                // Remap the random variable (fail case).
                random.x = remapFail(random.x, P);

                // Sample and evaluate the Lambertian BRDF, for this dielectric diffuse lobe. Scale
                // the result with the base factor.
                result = sampleLambertianBRDF(V, diffuseColor, random, L, pdf);
                result *= base;

                // Set the lobe ID.
                lobeID = DIFFUSE_LOBE;
            }
        }
    }

    // Transform the sampled light direction to world space.
    Lw = tangentToWorld(L, X, Y, N);

    // Return the result multipled by the cosine term (NdotL or L.z) from the rendering equation.
    return result * abs(L.z);
}

#endif // __REFERENCE_MATERIAL_H__
