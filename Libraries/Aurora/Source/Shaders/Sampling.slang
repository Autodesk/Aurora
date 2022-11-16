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
#ifndef __SAMPLING_H__
#define __SAMPLING_H__

#include "Globals.slang"

// Generates a uniformly distributed random direction.
float3 sampleUniformDirection(float2 random, out float pdf)
{
    // Create a point on the unit sphere, from the uniform random variables. Since this is a unit
    // sphere, this can also be used as a direction.
    // NOTE: See "Ray Tracing Gems" section 16.5 for details.
    float cosTheta   = 1.0f - 2.0f * random[1];
    float sinTheta   = sqrt(1.0f - cosTheta * cosTheta);
    float phi        = 2.0f * M_PI * random[0];
    float3 direction = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // The PDF is uniform, at one over the area, i.e. the total solid angle of 4*Pi.
    pdf = 0.25f * M_PI_INV;

    return direction;
}

// Generates a random direction in the cosine-weighted hemisphere above the specified normal. This
// provides a PDF value ("probability density function") which is the *relative* probability that
// the returned direction will be chosen.
float3 sampleHemisphere(float2 random, float3 normal, out float pdf)
{
    // Uniformly sample a direction, i.e. over a sphere.
    float3 direction = sampleUniformDirection(random, pdf);

    // To transform that into a sample from a cosine-weighted hemisphere over the normal, treat the
    // sphere as tangent to the surface: add the normal to the direction and normalize it. The PDF
    // is cos(theta) / PI, so use a dot product to compute cos(theta).
    // NOTE: See "Ray Tracing in One Weekend" for details.
    direction = normalize(normal + direction);
    pdf       = dot(normal, direction) * M_PI_INV;

    return direction;
}

// Generates a random direction in the cosine-weighted hemisphere above the tangent-space normal
// (0.0, 0.0, 1.0).
float3 sampleHemisphere(float2 random, out float pdf)
{
    // Generate a uniform sample on the 2D disk, and project it up to the hemisphere. This results
    // in a cosine-weighted hemisphere sample.
    float r          = sqrt(random[0]);
    float phi        = 2.0f * M_PI * random[1];
    float3 direction = float3(r * cos(phi), r * sin(phi), sqrt(1.0f - random[0]));

    // The PDF is the cosine of the angle (i.e. the Z component here) divided by Pi.
    pdf = direction.z * M_PI_INV;

    return direction;
}

// Generates a uniformly distributed random direction in a cone with the specified direction and
// angular radius.
float3 sampleCone(float2 random, float3 direction, float cosThetaMax)
{
    // Compute arbitrary basis vectors based on the cone direction.
    float3 x_basis, y_basis;
    buildBasis(direction, x_basis, y_basis);

    // Compute angles for the direction based on the random variables.
    // NOTE: See "Ray Tracing Gems" section 16.5 for details.
    float cosTheta = (1.0f - random[0]) + random[0] * cosThetaMax;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    float phi      = random[1] * 2 * M_PI;

    // Construct and return the direction vector.
    return x_basis * cos(phi) * sinTheta + y_basis * sin(phi) * sinTheta + direction * cosTheta;
}

// Generates a uniformly distributed random point on a disk with the specified radius.
// NOTE: See "Ray Tracing Gems" section 16.5 for details.
float2 sampleDisk(float2 random, float radius)
{
    float r   = radius * sqrt(random[0]);
    float phi = 2.0f * M_PI * random[1];

    return r * float2(cos(phi), sin(phi));
}

// Computes the weight for scaling contributions for multiple importance sampling, which is used to
// reduce variance with MIS.
// NOTE: See PBRT 3E 13.10.1. This is specifically the "power heuristic." nf and ng are typically 1.
float computeMISWeight(int nf, float fPDF, int ng, float gPDF)
{
    float f = nf * fPDF;
    float g = ng * gPDF;

    return (f * f) / (f * f + g * g);
}

#endif // __SAMPLING_H__
