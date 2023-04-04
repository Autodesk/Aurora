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
#ifndef __RANDOM_H__
#define __RANDOM_H__

// Choose a random number generator:
// - LCG: A pseudorandom LCG to generate independent random numbers.
// - PCG2D: A pseudorandom version of PCG to generate two distributed random numbers.
//#define RNG_LCG
#define RNG_PCG2D

// Define initRand() and randomNext() functions based on the chosen random number generator.
#if defined(RNG_LCG)
#define initRand lcgInit
#define randomNext2D lcgNext2D
#endif
#if defined(RNG_PCG2D)
#define initRand pcg2DInit
#define randomNext2D pcg2DNext2D
#endif

// State used for random number generation (RNG).
struct Random
{
    uint2 state;
};

// Generates a random 32-bit integer from two seed values, with the Tiny Encryption Algorithm (TEA).
// NOTE: Based on
// https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_jitter_cam
uint tea(uint val0, uint val1)
{
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 16; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Generates a random 32-bit integer from a seed value, using a linear congruential generator (LCG).
// NOTE: Based on "Hash Functions for GPU Rendering" @ http://www.jcgt.org/published/0009/03/02.
uint lcg(uint p)
{
    return p * 1664525u + 1013904223u;
}

// Initializes a random number generator for LCG.
Random lcgInit(uint sampleIndex, uint2 screenSize, uint2 screenCoords)
{
    Random rng;

    // Compute a random integer as a seed for LCG, because LCG does not behave well with consecutive
    // seeds. This is based on the pixel index and sample index, to prevent correlation across space
    // and time.
    uint pixelIndex = screenCoords.y * screenSize.x + screenCoords.x;
    rng.state.x     = tea(pixelIndex, sampleIndex);

    return rng;
}

// Computes the next LCG value, updating the specified state value.
float lcgNext(inout uint state)
{
    // Use the lower 24 bits of an LCG result, with the RNG as a seed.
    state = lcg(state) & 0x00FFFFFF;

    // Produce a floating-point number in the range [0.0, 1.0), from the 24-bit state.
    return state / float(0x01000000);
}

// Computes the next LCG value pair, updating the specified RNG.
float2 lcgNext2D(inout Random rng)
{
    return float2(lcgNext(rng.state.x), lcgNext(rng.state.x));
}

// Initializes a random number generator for PCG2D.
Random pcg2DInit(uint sampleIndex, uint2 screenSize, uint2 screenCoords)
{
    Random rng;

    // Initialize the state based on the pixel location and sample index, to prevent correlation
    // across space and time.
    // NOTE: Unlike LCG, PCG2D does behave well with consecutive seeds, so the seed does not need to
    // be scrambled. See https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering.
    rng.state.x = screenCoords.y * screenSize.x + screenCoords.x;
    rng.state.y = sampleIndex;

    return rng;
}

// Generates two random 32-bit integers from seed values, using a two-dimensional permuted
// congruential generator (PCG).
// NOTE: Based on "Hash Functions for GPU Rendering" @ http://www.jcgt.org/published/0009/03/02.
uint2 pcg2D(inout uint2 state)
{
    // Retain the current state, and advance to the next state with a simple LCG.
    uint2 v = state;
    state = state * 1664525u + 1013904223u;

    // Apply the PCG2D transformation to the current state to produce the result pair of values.
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v   = v ^ (v >> 16u);
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v   = v ^ (v >> 16u);

    return v;
}

// Computes the next PCG2D value pair, updating the specified RNG.
float2 pcg2DNext2D(inout Random rng)
{
    rng.state = pcg2D(rng.state);

    // Produce floating-point numbers in the range [0.0, 1.0).
    return float2(rng.state / float(0xFFFFFFFFu));
}

// Generates two uniformly distributed numbers in the range [0.0, 1.0), using the specified random
// number generator.
// NOTE: The two numbers may be correlated to provide an even distribution across the 2D range,
// which is why this is a single 2D function instead of two calls to a 1D function.
float2 random2D(inout Random rng)
{
    return randomNext2D(rng);
}

#endif // __RANDOM_H__