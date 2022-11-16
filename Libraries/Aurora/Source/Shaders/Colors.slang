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
#ifndef __COLORS_H__
#define __COLORS_H__

// Linearizes an sRGB color.
// NOTE: Use this only for hardcoded colors in the shader. All other color inputs should already be
// linearized.
float3 sRGBtoLinear(float3 value)
{
    return value * (value * (value * 0.305306011f + 0.682171111f) + 0.012522878f);
}

// Computes the perceived luminance of a color.
float luminance(float3 value)
{
    return dot(value, float3(0.2125f, 0.7154f, 0.0721f));
}

// Converts from linear color space to sRGB (gamma correction) for display.
// NOTE: Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html.
float3 linearTosRGB(float3 color)
{
    float3 sq1 = sqrt(color);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);

    return 0.662002687f * sq1 + 0.684122060f * sq2 - 0.323583601f * sq3 - 0.0225411470f * color;
}

// Applies the ACES filmic tone mapping curve to the color.
// NOTE: Based on https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve.
float3 toneMapACES(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

#endif // __COLORS_H__