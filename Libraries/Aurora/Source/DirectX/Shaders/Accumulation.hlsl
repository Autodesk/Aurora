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

// Declare the root signature for the compute shader.
// NOTE: DESCRIPTORS_VOLATILE is specified for root signature 1.0 behavior, where descriptors do not
// have to be initialized in advance.
#define ROOT_SIGNATURE                                                                             \
    "RootFlags(0),"                                                                                \
    "DescriptorTable(UAV(u0, numDescriptors = 10, flags = DESCRIPTORS_VOLATILE)), "                \
    "RootConstants(b0, num32BitConstants = 2)"

// Source (input) and destination (output) textures.
RWTexture2D<float4> gAccumulation : register(u0);
RWTexture2D<float4> gDirect : register(u1);
RWTexture2D<float> gDepthNDC : register(u2);
RWTexture2D<float> gDepthView : register(u3);
RWTexture2D<float4> gNormalRoughness : register(u4);
RWTexture2D<float4> gBaseColorMetalness : register(u5);
RWTexture2D<float4> gDiffuse : register(u6);
RWTexture2D<float4> gGlossy : register(u7);
RWTexture2D<float4> gDiffuseDenoised : register(u8);
RWTexture2D<float4> gGlossyDenoised : register(u9);

// Layout of accumulation properties.
struct Accumulation
{
    uint sampleIndex;
    bool isDenoisingEnabled;
};

// Constant buffer of accumulation values.
ConstantBuffer<Accumulation> gSettings : register(b0);

// A compute shader that accumulates path tracing results, optionally with denoising.
[RootSignature(ROOT_SIGNATURE)][numthreads(1, 1, 1)] void Accumulation(uint3 threadID
                                                                       : SV_DispatchThreadID)
{
    uint sampleIndex = gSettings.sampleIndex;

    // Get the screen coordinates (2D) from the thread ID, and the color / alpha as the result.
    // Treat the color as the direct lighting value, optionally used below.
    float2 screenCoords = threadID.xy;
    float4 result       = gDirect[screenCoords];
    float3 direct       = result.rgb;

    // Combine data from textures if denoising is enabled. Otherwise the "direct" texture has the
    // complete path tracing output.
    if (gSettings.isDenoisingEnabled)
    {
        // Collect the necessary values. This includes a "hit factor" which is 0 for a background
        // sample and 1 for a surface sample ("hit"). This is detected by testing for an infinite
        // value in the view depth texture.
        float hitFactor        = gDepthView[screenCoords].r != 1.#INF;
        float3 baseColor       = gBaseColorMetalness[screenCoords].rgb;
        float3 denoisedDiffuse = gDiffuseDenoised[screenCoords].rgb * hitFactor;
        float3 denoisedGlossy  = gGlossyDenoised[screenCoords].rgb * hitFactor;

        // Combine the following:
        // - Direct lighting. This also includes the environment background.
        // - The denoised diffuse radiance, modulated by the base color (albedo).
        // - The denoised glossy radiance.
        result.rgb = direct + denoisedDiffuse * baseColor + denoisedGlossy;
    }

    // If the sample index is greater than zero, blend the new result color with the previous
    // accumulation color.
    if (sampleIndex > 0)
    {
        // Get the previous result. If it has an infinity component, then it represents an error
        // pixel, and it should remain unmodified. Otherwise blend it with the new result.
        float4 prevResult = gAccumulation[screenCoords];
        if (any(isinf(prevResult)))
        {
            result = prevResult;
        }
        else
        {
            // Compute a blend factor (between the previous and new result) based on the sample
            // index, with the new result having less influence with an increasing sample index,
            // e.g. with sample index #4 (the 5th sample), the final result is 4/5 of the previous
            // (accumulated) result and 1/5 of the new result. If denoising is enabled, this should
            // be treated as temporal accumulation, with the new result having a fixed influence, so
            // that older results are eventually discarded.
            static const float TEMPORAL_BLEND_FACTOR = 0.1f;
            float t =
                gSettings.isDenoisingEnabled ? TEMPORAL_BLEND_FACTOR : 1.0f / (sampleIndex + 1);

            // Blend between the previous result and the new result using the factor.
            result = lerp(prevResult, result, t);
        }
    }

    // Write to the output (accumulation) buffer.
    gAccumulation[screenCoords] = result;
}