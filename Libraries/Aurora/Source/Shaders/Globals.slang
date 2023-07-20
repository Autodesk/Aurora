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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

// Math constants.
#define M_PI 3.1415926535897932384626433832795f
#define M_PI_INV 1.0 / 3.1415926535897932384626433832795
#define M_FLOAT_EPS 0.000001f
#define M_RAY_TMIN 0.01f

// Miss shader indices.
#if DIRECTX
#define kMissNull 0       // for a disabled miss shader (no effect)
#define kMissBackground 1 // for the background, visible by primary rays
#define kMissRadiance 2   // for lighting (radiance)
#define kMissShadow 3     // for shadow rays, forced to opaque
#else
// In HGI backend, backround and radiance miss shader are the same.
#define kMissBackground 0 // for the background, visible by primary rays
#define kMissRadiance 1   // for lighting (radiance)
#define kMissShadow 2     // for shadow rays, forced to opaque
#endif

// Importance sampling modes. This is set with the IMPORTANCE_SAMPLING_MODE preprocessor symbol.
#define IMPORTANCE_SAMPLING_BSDF 0        // sample the BSDF alone
#define IMPORTANCE_SAMPLING_ENVIRONMENT 1 // sample the environment alone
#define IMPORTANCE_SAMPLING_MIS 2         // sample both the BSDF and environment
#if !defined(IMPORTANCE_SAMPLING_MODE)
#if DIRECTX
#define IMPORTANCE_SAMPLING_MODE IMPORTANCE_SAMPLING_MIS
#else
#define IMPORTANCE_SAMPLING_MODE IMPORTANCE_SAMPLING_BSDF
#endif

#endif

// Black and white constant values, which can also be used as a float3.
#define BLACK 0.0f
#define WHITE 1.0f

// Neither INIFITIY or sizeof supported in Slang
#define INFINITY 10.0e+99 // Set to larger that largest float32, will get converted to infinity
#define WORD_SIZE 4

// Returns whether the specified color is black, i.e. all zero components.
bool isBlack(float3 color)
{
    return all(color == BLACK);
}

// Square utility function.
float sqr(float x)
{
    return x * x;
}

// Builds basis vectors (tangent and bitangent) from a normal vector.
void buildBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
    bitangent = abs(normal.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    tangent   = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);
}

// Rebuild tangent and bitangent basis from new normal.
void rebuildBasis(float3 normal, in out float3 tangent, in out float3 bitangent)
{
    bitangent = abs(dot(bitangent, normal)) < 0.999f ? bitangent : tangent;
    tangent   = normalize(cross(bitangent, normal));
    bitangent = cross(normal, tangent);
}

// Compute bitangent from a normal and tangent vector.
float3 computeBitangent(float3 normal, float3 tangent)
{
    return cross(normal, tangent);
}

#endif // __GLOBALS_H__