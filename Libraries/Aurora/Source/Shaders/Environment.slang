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
#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include "Colors.slang"
#include "Globals.slang"
#include "Sampling.slang"

// Layout of environment constant properties.
struct EnvironmentConstants
{
    float3 lightTop;
    float _padding1;
    float3 lightBottom;
    float lightTexLuminanceIntegral;
    float4x4 lightTransform;
    float4x4 lightTransformInv;
    float3 backgroundTop;
    float _padding3;
    float3 backgroundBottom;
    float _padding4;
    float4x4 backgroundTransform;
    bool backgroundUseScreen;
    bool hasLightTex;
    bool hasBackgroundTex;
};

// Layout of an alias map entry.
// NOTE: Padding to 16 byte (float4) alignment is added for best performance.
struct AliasEntry
{
    uint alias;
    float probability;
    float pdf;
    float _padding1;
};

// Environment properties.
struct Environment
{
    EnvironmentConstants constants;
    Texture2D<float4> backgroundTexture;
    SamplerState sampler;
    Texture2D<float4> lightTexture;

#if DIRECTX
    StructuredBuffer<AliasEntry> aliasMap;
#endif
};

// Computes UV coordinates for an image from a direction. The image is assumed to have lat-long
// layout, with the center of the image in the -Z direction, and the top in the +Y direction.
float2 directionToLatLongUV(float3 direction)
{
    float2 uv;
    uv.x = atan2(direction.x, -direction.z) * M_PI_INV * 0.5f + 0.5f;
    uv.y = -asin(direction.y) * M_PI_INV + 0.5f;

    return uv;
}

// Computes a direction from UV coordinates for an image. The image is assumed to have lat-long
// layout, with the center of the image in the -Z direction, and the top in the +Y direction.
float3 latLongUVToDirection(float2 uv)
{
    // Compute spherical coordinates.
    float phi   = (uv.x + 0.25f) * 2.0f * M_PI; // longitude
    float theta = uv.y * M_PI;                  // latitude

    // Prepare values for computing the direction.
    float sinPhi, cosPhi, sinTheta, cosTheta;
    sincos(phi, sinPhi, cosPhi);
    sincos(theta, sinTheta, cosTheta);

    // Compute the direction in Cartesian coordinates.
    return float3(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
}

// Compute UV from direction vector.
float2 computeEnvironmentUV(float4x4 transform, bool useScreen, float3 direction)
{
    // Compute the sampling coordinates, either using screen coordinates or lat-long layout.
    float2 uv;
    if (useScreen)
    {
        // Compute the texture coordinates from the dispatch index and dimensions.
        float2 screenCoords = float2(DispatchRaysIndex().xy);
        float2 screenSize   = float2(DispatchRaysDimensions().xy);
        uv                  = screenCoords / screenSize;
    }
    else
    {
        // Transform the direction vector, and compute UV coordinates from it.
        direction = normalize(mul(transform, float4(direction, 0.0f)).xyz);
        uv        = directionToLatLongUV(direction);
    }

    return uv;
}

// Evaluate a procedural gradient environment with the specified properties, with the specified UV.
float3 evaluateEnvironmentGradient(float2 uv, float3 topColor, float3 bottomColor)
{
    // Sample the texture, or interpolate the gradient colors using the Y coordinate.
    return lerp(topColor, bottomColor, uv.y);
}

// Evaluate a texture environment with the specified properties, with the specified UV.
float3 evaluateEnvironmentTexture(float2 uv, Texture2D<float4> texture, SamplerState sampler)
{
    // Sample the texture, or interpolate the gradient colors using the Y coordinate.
    return texture.SampleLevel(sampler, uv, 0.0f).rgb;
}

// Evaluate the specified environment in the specified direction, optionally as a background.
float3 evaluateEnvironment(Environment environment, float3 direction, bool asBackground)
{

    EnvironmentConstants constants = environment.constants;
    
    if (asBackground)
    {
        float2 uv = computeEnvironmentUV(
            constants.backgroundTransform, constants.backgroundUseScreen, direction);
        if (constants.hasBackgroundTex)
        {
            return evaluateEnvironmentTexture(
                uv, environment.backgroundTexture, environment.sampler);
        }
        else
        {
            return evaluateEnvironmentGradient(
                uv, constants.backgroundTop, constants.backgroundBottom);
        }
    }
    else
    {
        float2 uv = computeEnvironmentUV(constants.lightTransform, false, direction);

        if (constants.hasLightTex)
        {
            return evaluateEnvironmentTexture( uv,
                environment.lightTexture, environment.sampler);
        }
        else
        {
            return evaluateEnvironmentGradient(uv, constants.lightTop, constants.lightBottom);
        }
    }

}

// Samples the specified environment, returning a sample (light) direction and corresponding
// probability density function (PDF).
float3 sampleEnvironment(Environment environment, float2 random, out float3 L, out float pdf)
{
    // Sample the environment as either a texture or a gradient.
    EnvironmentConstants constants = environment.constants;
#if DIRECTX
    if (constants.hasLightTex)
    {
        // Get the textures dimensions and texel count.
        uint width, height, count;
        environment.lightTexture.GetDimensions(width, height);
        count = width * height;

        // Select an alias map entry at random, corresponding to a texel in the environment texture.
        // If the entry's probability does not exceed another random value, use the entry's alias
        // instead, corresponding to a different texel.
        uint index       = min(count * random[0], count - 1);
        AliasEntry entry = environment.aliasMap[index];
        if (entry.probability < random[1])
        {
            index = entry.alias;
            entry = environment.aliasMap[index];
        }
        pdf = entry.pdf;

        // Compute texture coordinates in texel space, convert to a direction, and transform it.
        // NOTE: The inverse light transform is used because the direction is computed here as part
        // of sampling (an output), rather than being specified as for evaluation (an input).
        int3 xy = int3(index % width, index / width, 0);
        L       = latLongUVToDirection(float2((xy.x + 0.5f) / width, (xy.y + 0.5f) / height));
        L       = normalize(mul(constants.lightTransformInv, float4(L, 0.0f)).xyz);

        // Load from the texture using the texture coordinates and return as the radiance.
        // NOTE: This does not perform texel interpolation, but that could be done with using
        // SampleLevel() with a random location inside the lat-long patch represented by the texel.
        return environment.lightTexture.Load(xy).rgb;
    }
    else
#endif
    {
        // Uniformly sample a direction.
        L = sampleUniformDirection(random, pdf);

        // Transform the direction vector, and compute UV coordinates from it.
        // NOTE: The inverse light transform is used because the direction is computed here as part
        // of sampling (an output), rather than being specified as for evaluation (an input).
        L         = normalize(mul(constants.lightTransformInv, float4(L, 0.0f)).xyz);
        float2 uv = directionToLatLongUV(L);

        // Interpolate the gradient colors using the Y coordinate and return as the radiance.
        return lerp(constants.lightTop, constants.lightBottom, uv.y);
    }
}

#endif // __ENVIRONMENT_H__
