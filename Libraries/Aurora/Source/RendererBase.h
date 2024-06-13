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
#pragma once

#include "AssetManager.h"
#include "Properties.h"
#include "SceneBase.h"

BEGIN_AURORA

class SceneBase;

// Property names as constants.
static const string kLabelIsResetHistoryEnabled       = "isResetHistoryEnabled";
static const string kLabelIsDenoisingEnabled          = "isDenoisingEnabled";
static const string kLabelIsDiffuseOnlyEnabled        = "isDiffuseOnlyEnabled";
static const string kLabelDebugMode                   = "debugMode";
static const string kLabelMaxLuminance                = "maxLuminance";
static const string kLabelTraceDepth                  = "traceDepth";
static const string kLabelIsToneMappingEnabled        = "isToneMappingEnabled";
static const string kLabelIsGammaCorrectionEnabled    = "isGammaCorrectionEnabled";
static const string kLabelIsAlphaEnabled              = "alphaEnabled";
static const string kLabelBrightness                  = "brightness";
static const string kLabelUnits                       = "units";
static const string kLabelImportanceSamplingMode      = "importanceSamplingMode";
static const string kLabelIsFlipImageYEnabled         = "isFlipImageYEnabled";
static const string kLabelIsReferenceBSDFEnabled      = "isReferenceBSDFEnabled";
static const string kLabelIsForceOpaqueShadowsEnabled = "isForceOpaqueShadowsEnabled";

// The debug modes include:
// - 0 Output (accumulation)
// - 1 Output with Errors
// - 2 View Depth
// - 3 Normal
// - 4 Base Color
// - 5 Roughness
// - 6 Metalness
// - 7 Diffuse
// - 8 Diffuse Hit Distance
// - 9 Glossy
// - 10 Glossy Hit Distance
static const int kDebugModeErrors = 1;
static const int kMaxDebugMode    = 10;

// Importance sampling mode options as constants.
static const int kImportanceSamplingModeBSDF        = 0;
static const int kImportanceSamplingModeEnvironment = 1;
static const int kImportanceSamplingModeMIS         = 2;

// A base class for implementations of IRenderer.
class RendererBase : public IRenderer, public FixedValues
{
public:
    /*** Lifetime Management ***/

    RendererBase(uint32_t activeTaskCount);

    /*** IRenderer Functions ***/

    void setOptions(const Properties& option) override;
    IValues& options() override { return *this; };
    void setCamera(const mat4& view, const mat4& projection, float focalDistance = 1.0f,
        float lensRadius = 0.0f) override;
    void setCamera(
        const float* view, const float* proj, float focalDistance, float lensRadius) override;
    void setFrameIndex(int frameIndex) override;

    /*** Functions ***/

    bool isValid() { return _isValid; }
    void valuesToProperties(const FixedValueSet& values, Properties& props) const;
    void propertiesToValues(const Properties& properties, IValues& values);

    unique_ptr<AssetManager>& assetManager() { return _pAssetMgr; }

// TODO: Destruction via shared_ptr is not safe, we should have some kind of kill list system, but
// can't seem to get it to work.
#if 0
    void destroyImage(IImagePtr& pImg)
    {
        _imageDestroyList.push_back(pImg);
        pImg.reset();
    }
#endif

    // The max trace depth, for recursion in ray tracing.
    static const int kMaxTraceDepth;

protected:
    // Layout of per-frame parameters.
    // Must match the GPU version Frame.slang.
    struct FrameData
    {
        // The view-projection matrix.
        mat4 cameraViewProj;

        // The inverse view matrix, also transposed. The *rows* must have the desired vectors:
        // right, up, front, and eye position. HLSL array access with [] returns rows, not columns,
        // hence the need for the matrix to be supplied transposed.
        mat4 cameraInvView;

        // The dimensions of the view (in world units) at a distance of 1.0 from the camera, which
        // is useful to build ray directions.
        vec2 viewSize;

        // Whether the camera is using an orthographic projection. Otherwise a perspective
        // projection is assumed.
        int isOrthoProjection = 0;

        // The distance from the camera for sharpest focus, for depth of field.
        float focalDistance = 0.f;

        // The diameter of the lens for depth of field. If this is zero, there is no depth of field,
        // i.e. pinhole camera.
        float lensRadius = 0.f;

        // The size of the scene, specifically the maximum distance between any two points in the
        // scene.
        float sceneSize = 0.f;

        // Whether shadow evaluation should treat all objects as opaque, as a performance
        // optimization.
        int isForceOpaqueShadowsEnabled = 0;

        // Whether to write the NDC depth result to an output texture.
        int isDepthNDCEnabled = 0;

        // Whether to render the diffuse material component only.
        int isDiffuseOnlyEnabled = 0;

        // Whether to display shading errors as bright colored samples.
        int isDisplayErrorsEnabled = 0;

        // Whether denoising is enabled, which affects how path tracing is performed.
        int isDenoisingEnabled = 0;

        // Whether to write the AOV data required for denoising.
        int isDenoisingAOVsEnabled = 0;

        // The maximum recursion level (or path length) when tracing rays.
        int traceDepth = 0;

        // The maximum luminance for path tracing samples, for simple firefly clamping.
        float maxLuminance = 0.f;

        // Pad to 16 byte boundary.
        vec2 _padding1;

        // Current light data for scene (duplicated each frame in flight.)
        SceneBase::LightData lights;
        
        // frameIndex
        int frameIndex;
    };

    // Accumulation settings GPU data.
    struct Accumulation
    {
        unsigned int sampleIndex;
        unsigned int isDenoisingEnabled;
    };

    // Post-processing settings GPU data.
    struct PostProcessing
    {
        vec3 brightness;
        int debugMode;
        vec2 range;
        int isDenoisingEnabled;
        int isToneMappingEnabled;
        int isGammaCorrectionEnabled;
        int isAlphaEnabled;
    };

    // Sample settings GPU data.
    struct SampleData
    {
        // The sample index (iteration) for the frame, for progressive rendering.
        uint sampleIndex;

        // An offset to apply to the sample index for seeding a random number generator.
        uint seedOffset;
    };

    FrameData _frameData;
    Accumulation _accumData;
    SampleData _sampleData;
    PostProcessing _postProcessingData;

    bool updateFrameDataGPUStruct(FrameData* pStaging = nullptr);
    bool updatePostProcessingGPUStruct(PostProcessing* pStaging = nullptr);
    bool updateAccumulationGPUStruct(uint32_t sampleIndex, Accumulation* pStaging = nullptr);

    /*** Protected Variables ***/

// TODO: Destruction via shared_ptr is not safe, we should have some kind of kill list system, but
// can't seem to get it to work.
#if 0
    vector<IImagePtr> _imageDestroyList;
#endif
    shared_ptr<SceneBase> _pScene;

    bool _isValid        = false;
    uint32_t _taskCount  = 0;
    uint32_t _taskIndex  = 0;
    uint64_t _taskNumber = 0;
    mat4 _cameraView;
    mat4 _cameraProj;
    float _focalDistance = 1.0f;
    float _lensRadius    = 0.0f;
    int _frameIndex      = 0;

    // Asset manager for loading external assets.
    unique_ptr<AssetManager> _pAssetMgr;
};
MAKE_AURORA_PTR(RendererBase);

END_AURORA
