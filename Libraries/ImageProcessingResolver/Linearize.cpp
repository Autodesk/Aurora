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
#include "pch.h"

#include "Linearize.h"

float reverseSpectrumToneMappingLow(float x)
{
    const float a  = 0.244925773f;
    const float t1 = -0.631960243f;
    const float t2 = -4.251792105f;
    const float t3 = 3.841721426f;
    const float d0 = -0.005002772f;
    const float d1 = -0.525010335f;
    const float d2 = -0.885018509f;
    const float d3 = 1.153153531f;

    const float x2 = x * x;
    const float x3 = x2 * x;
    return a * (t1 * x + t2 * x2 + t3 * x3) / (d0 + d1 * x + d2 * x2 + d3 * x3);
}

float reverseSpectrumToneMappingHigh(float x)
{
    const float a  = 0.514397858f;
    const float t1 = -0.553266341f;
    const float t2 = -2.677674970f;
    const float t3 = 2.903543727f;
    const float d0 = -0.005057344f;
    const float d1 = -0.804910686f;
    const float d2 = -1.427186761f;
    const float d3 = 2.068910419f;

    const float x2 = x * x;
    const float x3 = x2 * x;
    return a * (t1 * x + t2 * x2 + t3 * x3) / (d0 + d1 * x + d2 * x2 + d3 * x3);
}

void canonInverse(
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf, float exposure)
{
    // Work out channel count from HIO format.
    int numChannels = pxr::HioGetComponentCount(imageData.format);

    // Compute 2^exposure.
    float exposure2 = exp2(exposure);

    // Image dims and pixel count.
    int imagewidth  = imageData.width;
    int imageheight = imageData.height;
    int pixels      = imagewidth * imageheight * numChannels;

    // If the input format is LDR we must convert to float first, use temp buffer.
    bool isConversionRequired = imageData.format == pxr::HioFormatUNorm8Vec3srgb;
    unsigned char* srcBytes   = imageBuf.data();
    float* dst                = reinterpret_cast<float*>(srcBytes);
    if (!dst)
    {
        return;
    }
    vector<unsigned char> newPixels;
    if (isConversionRequired)
    {
        newPixels.resize(pixels * sizeof(float));
        dst = reinterpret_cast<float*>(newPixels.data());
    }

    const float spectrumTonemapMin = -2.152529302052785809f;
    const float spectrumTonemapMax = 1.163792197947214113f;

    const float lowHighBreak = 0.9932f;

    const float shift = 0.18f;
    for (int i = 0; i < pixels; ++i)
    {
        float val;
        if (isConversionRequired)
            val = float(srcBytes[i]) / 255.0f;
        else
            val = dst[i];

        if (val < lowHighBreak)
        {
            val = reverseSpectrumToneMappingLow(val);
        }
        else
        {
            val = reverseSpectrumToneMappingHigh(val);
        }
        val = pxr::GfClamp(val, 0.0f, 1.0f);
        val = val * (spectrumTonemapMax - spectrumTonemapMin) + spectrumTonemapMin;
        val = pxr::GfPow(10.0f, val);
        val /= exposure2;
        val *= shift;

        dst[i] = val;
    }

    // Copy the temp buffer to the output buffer, if used.
    if (isConversionRequired)
    {
        imageBuf         = newPixels;
        imageData.data   = imageBuf.data();
        imageData.format = pxr::HioFormatFloat32Vec3;
    }
}

bool linearize(const AssetCacheEntry& cacheEntry, pxr::HioImageSharedPtr const /*pImage*/,
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf)
{

    // If not RGB float32 return false;
    if (imageData.format != pxr::HioFormatFloat32Vec3 &&
        imageData.format != pxr::HioFormatUNorm8Vec3srgb)
    {
        AU_WARN("Unsupported format for linearization");
        return false;
    }

    // Get exposure from URI query field.
    float exposure = 0.0f;
    cacheEntry.getQuery("exposure", &exposure);

    // Run inverse tone mapping to linearize image.
    canonInverse(imageData, imageBuf, exposure);

    // Return success.
    return true;
}
