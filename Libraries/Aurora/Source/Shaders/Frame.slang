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
#ifndef __FRAME_H__
#define __FRAME_H__

// Layout of sample parameters.
struct SampleData
{
    // The sample index (iteration) for the frame, for progressive rendering.
    uint sampleIndex;

    // An offset to apply to the sample index for seeding a random number generator.
    uint seedOffset;
};

// Layout of per-frame parameters.
struct FrameData
{
    // The view-projection matrix.
    float4x4 cameraViewProj;

    // The inverse view matrix, also transposed. The *rows* must have the desired vectors: right,
    // up, front, and eye position. HLSL array access with [] returns rows, not columns, hence the
    // need for the matrix to be supplied transposed.
    float4x4 cameraInvView;

    // The dimensions of the view (in world units) at a distance of 1.0 from the camera, which is
    // useful to build ray directions.
    float2 viewSize;

    // Whether the camera is using an orthographic projection. Otherwise a perspective projection is
    // assumed.
    bool isOrthoProjection;

    // The distance from the camera for sharpest focus, for depth of field.
    float focalDistance;

    // The diameter of the lens for depth of field. If this is zero, there is no depth of field,
    // i.e. pinhole camera.
    float lensRadius;

    // The size of the scene, specifically the maximum distance between any two points in the scene.
    float sceneSize;

    float2 _padding1;

    // The direction of the global light.
    // NOTE: This is *toward* the light.
    float3 lightDir;

    float _padding2;

    // The color of the light (RGB) and its intensity (A).
    float4 lightColorAndIntensity;

    // The cosine of the light radius (as a disc).
    float lightCosRadius;

    // Whether shadow evaluation should treat all objects as opaque, as a performance optimization.
    bool isOpaqueShadowsEnabled;

    // Whether to write the NDC depth result to an output texture.
    bool isDepthNDCEnabled;

    // Whether to render the diffuse material component only.
    bool isDiffuseOnlyEnabled;

    // Whether to display shading errors as bright colored samples.
    bool isDisplayErrorsEnabled;

    // Whether denoising is enabled, which affects how path tracing is performed.
    bool isDenoisingEnabled;

    // Whether to write the AOV data required for denoising.
    bool isDenoisingAOVsEnabled;

    // The maximum recursion level (or path length) when tracing rays.
    int traceDepth;

    // The maximum luminance for path tracing samples, for simple firefly clamping.
    float maxLuminance;
};

#endif // __FRAME_H__
