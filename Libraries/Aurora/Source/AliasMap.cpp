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

#include "AliasMap.h"

BEGIN_AURORA

namespace AliasMap
{

// Computes the perceived luminance of a color.
static float computeLuminance(const vec3& value)
{
    static constexpr vec3 kLuminanceFactors = vec3(0.2125f, 0.7154f, 0.0721f);

    return dot(value, kLuminanceFactors);
}

void build(const float* pPixels, uvec2 dimensions, Entry* pOutputBuffer, size_t outputBufferSize,
    float& luminanceIntegralOut)
{
    // Calculate total number of pixels.
    unsigned int pixelCount = dimensions.x * dimensions.y;
    size_t bufferSize       = pixelCount * sizeof(Entry);

    // Ensure output buffer size is correct (is being put in GPU texture so must match exactly).
    AU_ASSERT(outputBufferSize == bufferSize,
        "Expected output buffer of size %d bytes, instead is %d bytes", pixelCount * sizeof(Entry),
        outputBufferSize);

    // A pair of luminance-related values, for a single pixel. Pixel luminance is used to determine
    // the relative importance of pixels, i.e. perceptually brighter pixels will be more likely to
    // be selected during sampling.
    struct LuminanceEntry
    {
        float luminance;        // the luminance of the pixel
        float luminanceAndArea; // the luminance of the pixel multiplied by its solid angle
    };

    // Prepare an array of temporary luminance data, used to build the alias map.
    vector<LuminanceEntry> luminanceData(pixelCount);

    // The luminance integral is computed below, and used to compute probabilities and PDF.
    luminanceIntegralOut = 0.0f;

    // Iterate the image pixels, storing the luminance and area-scaled luminance of each one.
    const float* pPixel   = pPixels;
    size_t luminanceIndex = 0;
    auto lonIncrement     = static_cast<float>(2.0f * M_PI / dimensions.x); // vertical (lat)
    auto latIncrement     = static_cast<float>(M_PI / dimensions.y);        // horizontal (lon)
    auto latAngle         = static_cast<float>(M_PI_2);
    for (unsigned int y = 0; y < dimensions.y; y++)
    {
        // Compute the solid angle covered by each pixel for current row (latitude) of the
        // environment image: (sin(upperAngle) - sin(lowerAngle)) * longitudeIncrement. The pixels
        // at the poles have a smaller solid angle than those at the equator.
        float solidAngle = (sin(latAngle) - sin(latAngle - latIncrement)) * lonIncrement;
        latAngle -= latIncrement;

        // Iterate the pixels of the current row, computing and accumulating luminance for each.
        for (unsigned int x = 0; x < dimensions.x; x++)
        {
            // Compute the luminance and area-scaled luminance for the current pixel. The area-
            // scaled luminance is actually a term in the luminance integral.
            float luminance        = computeLuminance(make_vec3(pPixel));
            float luminanceAndArea = solidAngle * luminance;

            // Store the luminance entry, and add the area-scaled luminance as another term in the
            // accumulated integral.
            luminanceData[luminanceIndex] = { luminance, luminanceAndArea };
            luminanceIntegralOut += luminanceAndArea;

            // Advance to the next pixel and luminance entry.
            pPixel += 3;
            luminanceIndex++;
        }
    }

    // Declare the generated alias map and a temporary index map.
    vector<Entry> aliasMap(pixelCount);
    vector<unsigned int> indexMap(pixelCount);

    // Iterate the pixels, preparing initial data for both the alias map and the index map.
    float average    = luminanceIntegralOut / pixelCount;
    unsigned int sml = 0, lrg = pixelCount;
    for (unsigned int i = 0; i < pixelCount; i++)
    {
        LuminanceEntry& luminanceEntry = luminanceData[i];

        // Initialize the probability for the alias map entry:
        // - Probability: This is a *normalized* probability, where the probability is the pixel's
        //   area-scaled luminance divided by the average of the luminance integral. In this way,
        //   there are "small" alias map entries with probabilities under 1.0 and "large" entries
        //   above 1.0 (below and above the average, respectively). These will be updated in the
        //   following loop.
        // - PDF: The PDF for an entry in a discrete distribution like this one (as there are a
        //   specific set of directions represented by the image pixels) is the pixel luminance
        //   divided by the luminance integral. In this way the integral of the PDF over the domain
        //   a sphere of directions) is 1.0, as required for Monte Carlo integration.
        Entry& aliasEntry = aliasMap[i];
        aliasEntry.prob   = luminanceEntry.luminanceAndArea / average;
        aliasEntry.pdf    = luminanceEntry.luminance / luminanceIntegralOut;

        // Compute an index based on the entry's probability, putting references to small
        // entries at the beginning of the index map and large entries at the end of the index
        // map. This also initializes each entry's alias index to point to itself.
        unsigned index   = aliasEntry.prob < 1.0f ? sml++ : --lrg;
        indexMap[index]  = i;
        aliasEntry.alias = i;
    }

    // At this point lrg points somewhere in the middle of the index map. Iterate the index map,
    // establishing alias indices for alias map entries, and distributing probability evenly. This
    // stops when the sml index reaches the lrg index, or when the lrg index reaches the end.
    // NOTE: The lrg index is updated in this loop, which is why the second check is needed.
    for (sml = 0; sml < lrg && lrg < pixelCount; sml++)
    {
        // Get the indices of the small and large alias map entries.
        unsigned int indexSmall = indexMap[sml];
        unsigned int indexLarge = indexMap[lrg];

        // Assign the small alias map entry to point to the large one. This pairing of alias map
        // entries is arbitrary (i.e. it depends on the original image data), but gives the desired
        // behavior.
        aliasMap[indexSmall].alias = indexLarge;

        // "Remove" probability from the large alias map entry, by the amount remaining in the small
        // entry, i.e. the amount below 1.0. This is effectively moved to the small entry, since the
        // small entry has an alias to the large entry.
        aliasMap[indexLarge].prob -= 1.0f - aliasMap[indexSmall].prob;

        // If the large entry now has a probability below 1.0, increment the lrg index. This means
        // the alias map entry is now treated as small, and will (later) receive an alias.
        lrg += aliasMap[indexLarge].prob < 1.0f ? 1 : 0;
    }

    // NOTE: At this point the alias map now has the total relative probability "distributed" evenly
    // across all entries as follows:
    // - Every small entry has an alias index for another entry from which it reduced probability.
    //   That other entry corresponds to a pixel with above average luminance.
    // - Even entries corresponding to pixels with above average luminance may have an alias to
    //   another pixel with above average luminance, because they had their probability reduced.
    // - Theoretically the large index should be close to the pixel count, i.e. every entry
    //   represents 1.0 relative probability between the corresponding pixel and the aliased pixel.
    // - However, due to numerical precision, the large index will likely be less than, but still
    //   relatively close to, the pixel count. The corresponding entries will have probabilities
    //   slightly above 1.0 and alias to themselves, which is fine when sampling in practice.
    //
    // See this extensive explanation of this technique, in the context of general discrete
    // probability distributions: https://www.keithschwarz.com/darts-dice-coins.

    // Copy to output buffer.
    memcpy(pOutputBuffer, aliasMap.data(), bufferSize);
}

} // namespace AliasMap

END_AURORA
