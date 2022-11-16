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
#pragma once

BEGIN_AURORA

namespace AliasMap
{

// An entry in the alias map. These are eventually used for sampling during rendering.
// NOTE: Padding to 16 byte (float4) alignment is added for best performance.
struct Entry
{
    unsigned int alias; // index of another entry accounting for the rest of the probability
    float prob;         // normalized probability of the entry
    float pdf;          // PDF value of the entry, for Monte Carlo integration
    vec1 _padding1;
};

// Creates an alias map from the pixel data for an environment image with lat-long layout. This is
// used for importance sampling from the environment image, by treating it as a discrete probability
// distribution.
// The luminance integral is also computed as part of alias map calculation.
void build(const float* pPixels, uvec2 dimensions, Entry* pOutputBuffer, size_t outputBufferSize,
    float& luminanceIntegralOut);

} // namespace AliasMap

END_AURORA
