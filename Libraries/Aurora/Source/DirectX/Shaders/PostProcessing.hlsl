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

#include "Colors.hlsli"

// Declare the root signature for the compute shader.
// NOTE: DESCRIPTORS_VOLATILE is specified for root signature 1.0 behavior, where descriptors do not
// have to be initialized in advance.
#define ROOT_SIGNATURE                                                                             \
    "RootFlags(0),"                                                                                \
    "DescriptorTable(UAV(u0, numDescriptors = 11, flags = DESCRIPTORS_VOLATILE)), "                \
    "RootConstants(b0, num32BitConstants = 10)"

// Debug display modes.
#define kDebugModeOff 0
#define kDebugModeErrors 1
#define kDebugModeDepthView 2
#define kDebugModeNormal 3
#define kDebugModeBaseColor 4
#define kDebugModeRoughness 5
#define kDebugModeMetalness 6
#define kDebugModeDiffuse 7
#define kDebugModeDiffuseHitDist 8
#define kDebugModeGlossy 9
#define kDebugModeGlossyHitDist 10

// Source (input) and destination (output) textures.
RWTexture2D<float4> gFinal : register(u0);
RWTexture2D<float4> gAccumulation : register(u1);
RWTexture2D<float4> gDirect : register(u2);
RWTexture2D<float> gDepthNDC : register(u3);
RWTexture2D<float> gDepthView : register(u4);
RWTexture2D<float4> gNormalRoughness : register(u5);
RWTexture2D<float4> gBaseColorMetalness : register(u6);
RWTexture2D<float4> gDiffuse : register(u7);
RWTexture2D<float4> gGlossy : register(u8);
RWTexture2D<float4> gDiffuseDenoised : register(u9);
RWTexture2D<float4> gGlossyDenoised : register(u10);

// Layout of post-processing properties.
struct PostProcessing
{
    float3 brightness;
    int debugMode;
    float2 range;
    int isDenoisingEnabled;
    int isToneMappingEnabled;
    int isGammaCorrectionEnabled;
    int isAlphaEnabled;
};

// Constant buffer of post-processing values.
ConstantBuffer<PostProcessing> gSettings : register(b0);

// Normalizes the specified view depth value to the scene range (front to back).
float normalizeDepthView(float depthView)
{
    return (depthView - gSettings.range.x) / (gSettings.range.y - gSettings.range.x);
}

// A compute shader that applies post-processing to the source texture.
[RootSignature(ROOT_SIGNATURE)][numthreads(1, 1, 1)] void PostProcessing(uint3 threadID
                                                                         : SV_DispatchThreadID)
{
    // Get the screen coordinates (2D) from the thread ID.
    float2 coords = threadID.xy;

    // Use the appropriate texture for output if a debug mode is enabled.
    float3 color                  = 0.0f;
    float alpha                   = gAccumulation[coords].a;
    bool isDenoisingEnabled       = gSettings.isDenoisingEnabled;
    bool isGammaCorrectionEnabled = gSettings.isGammaCorrectionEnabled;
    switch (gSettings.debugMode)
    {
    // Output (accumulation).
    case kDebugModeOff:
    case kDebugModeErrors:
        color = gAccumulation[coords].rgb;
        break;

    // View depth. Normalize the R channel of the view depth texture (grayscale).
    case kDebugModeDepthView:
        color                    = normalizeDepthView(gDepthView[coords].r).rrr;
        isGammaCorrectionEnabled = false;
        break;

    // Normal. Use the RGB channels of the normal-roughness texture.
    case kDebugModeNormal:
        color                    = gNormalRoughness[coords].rgb;
        isGammaCorrectionEnabled = false;
        break;

    // Base color. Use the RGB channels of the base-color-metalness texture.
    case kDebugModeBaseColor:
        color = gBaseColorMetalness[coords].rgb;
        break;

    // Roughness. Duplicate the A channel of the normal-roughness texture.
    case kDebugModeRoughness:
        color                    = gNormalRoughness[coords].aaa;
        isGammaCorrectionEnabled = false;
        break;

    // Metalness. Duplicate the A channel of the base-color-metalness texture.
    case kDebugModeMetalness:
        color                    = gBaseColorMetalness[coords].aaa;
        isGammaCorrectionEnabled = false;
        break;

    // Diffuse. Use the RGB channels of the diffuse texture.
    case kDebugModeDiffuse:
        color = (isDenoisingEnabled ? gDiffuseDenoised[coords] : gDiffuse[coords]).rgb;
        break;

    // Diffuse Hit Distance. Duplicate the A channel of the diffuse texture.
    case kDebugModeDiffuseHitDist:
        color = (isDenoisingEnabled ? gDiffuseDenoised[coords] : gDiffuse[coords]).aaa;
        isGammaCorrectionEnabled = false;
        break;

    // Glossy. Use the RGB channels of the glossy texture.
    case kDebugModeGlossy:
        color = (isDenoisingEnabled ? gGlossyDenoised[coords] : gGlossy[coords]).rgb;
        break;

    // Glossy Hit Distance. Duplicate the A channel of the glossy texture.
    case kDebugModeGlossyHitDist:
        color = (isDenoisingEnabled ? gGlossyDenoised[coords] : gGlossy[coords]).aaa;
        isGammaCorrectionEnabled = false;
        break;
    }

    // Perform post-processing for any output based on the accumulation (beauty) texture.
    if (gSettings.debugMode <= kDebugModeErrors)
    {
        // Apply brightness.
        color *= gSettings.brightness;

        // Apply ACES tone mapping or simple saturation.
        if (gSettings.isToneMappingEnabled)
        {
            color = toneMapACES(color);
        }
    }

    // Apply gamma correction.
    // NOTE: Gamma correction must be performed here as UAV textures don't support sRGB write.
    if (isGammaCorrectionEnabled)
    {
        color = linearTosRGB(saturate(color));
    }

    // Write to the final texture, optionally with alpha.
    gFinal[coords] = float4(color, gSettings.isAlphaEnabled ? alpha : 1.0f);
}