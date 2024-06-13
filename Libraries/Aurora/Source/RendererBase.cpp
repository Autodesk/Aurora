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

#include "AssetManager.h"
#include "RendererBase.h"
#include "SceneBase.h"

BEGIN_AURORA

// Create or get the property set (options) for the renderer.
static PropertySetPtr gpPropertySet;
static PropertySetPtr propertySet()
{
    if (gpPropertySet)
    {
        return gpPropertySet;
    }

    gpPropertySet = make_shared<PropertySet>();

    // Add the renderer options to the property set.
    gpPropertySet->add(kLabelIsResetHistoryEnabled, false);
    gpPropertySet->add(kLabelIsDenoisingEnabled, false);
    gpPropertySet->add(kLabelIsDiffuseOnlyEnabled, false);
    gpPropertySet->add(kLabelDebugMode, 0);
    gpPropertySet->add(kLabelMaxLuminance, 1000.0f);
    gpPropertySet->add(kLabelTraceDepth, 5);
    gpPropertySet->add(kLabelIsToneMappingEnabled, false);
    gpPropertySet->add(kLabelIsGammaCorrectionEnabled, true);
    gpPropertySet->add(kLabelIsAlphaEnabled, false);
    gpPropertySet->add(kLabelBrightness, vec3(1.0f, 1.0f, 1.0f));
    gpPropertySet->add(kLabelUnits, string("centimeter"));
    gpPropertySet->add(kLabelImportanceSamplingMode, kImportanceSamplingModeMIS);
    gpPropertySet->add(kLabelIsFlipImageYEnabled, true);
    gpPropertySet->add(kLabelIsReferenceBSDFEnabled, false);
    gpPropertySet->add(kLabelIsForceOpaqueShadowsEnabled, false);

    return gpPropertySet;
}

RendererBase::RendererBase(uint32_t taskCount) : FixedValues(propertySet()), _taskCount(taskCount)
{
    // Initialize the asset manager.
    _pAssetMgr = make_unique<AssetManager>();
    _pAssetMgr->enableVerticalFlipOnImageLoad(_values.asBoolean(kLabelIsFlipImageYEnabled));

    assert(taskCount > 0);
}

void RendererBase::setOptions(const Properties& options)
{
    propertiesToValues(options, *this);
}

void RendererBase::setCamera(
    const mat4& view, const mat4& projection, float focalDistance, float lensRadius)
{
    assert(focalDistance > 0.0f && lensRadius >= 0.0f);

    _cameraView    = view;
    _cameraProj    = projection;
    _focalDistance = focalDistance;
    _lensRadius    = lensRadius;
}

void RendererBase::setCamera(
    const float* view, const float* proj, float focalDistance, float lensRadius)
{
    assert(focalDistance > 0.0f && lensRadius >= 0.0f);

    _cameraView    = make_mat4(view);
    _cameraProj    = make_mat4(proj);
    _focalDistance = focalDistance;
    _lensRadius    = lensRadius;
}

void RendererBase::setFrameIndex(int frameIndex) {
    _frameIndex = frameIndex;
}

// Note that this handles strings differently than the implementation in SceneBase.
void RendererBase::propertiesToValues(const Properties& properties, IValues& values)
{
    for (auto& property : properties)
    {
        switch (property.second.type)
        {
        default:
        case PropertyValue::Type::Undefined:
            values.clearValue(property.first);
            break;

        case PropertyValue::Type::Bool:
            values.setBoolean(property.first, property.second._bool);
            break;

        case PropertyValue::Type::Int:
            values.setInt(property.first, property.second._int);
            break;

        case PropertyValue::Type::Float:
            values.setFloat(property.first, property.second._float);
            break;

        case PropertyValue::Type::Float2:
            AU_WARN("Cannot convert Float2 property.");
            break;

        case PropertyValue::Type::Float3:
            values.setFloat3(property.first, value_ptr(property.second._float3));
            break;

        case PropertyValue::Type::Float4:
            AU_WARN("Cannot convert Float4 property.");
            break;

        case PropertyValue::Type::String:
            values.setString(property.first, property.second._string);
            break;

        case PropertyValue::Type::Matrix4:
            values.setMatrix(property.first, value_ptr(property.second._matrix4));
            break;
        }
    }
}

bool RendererBase::updateFrameDataGPUStruct(FrameData* pStaging)
{
    FrameData frameData;

    // Get the camera properties:
    // - The view-projection matrix.
    // - The inverse view matrix, transposed.
    // - The dimensions of the view (in world units) at a distance of 1.0 from the camera, derived
    //   from the [0,0] and [1,1] elements of the projection matrix.
    // - Whether the camera uses an orthographic projection, defined by the [3,3] element of the
    //   projection matrix.
    // - Focal distance and lens radius, for depth of field.
    frameData.cameraViewProj    = _cameraProj * _cameraView;
    frameData.cameraInvView     = transpose(inverse(_cameraView));
    frameData.viewSize          = vec2(2.0f / _cameraProj[0][0], 2.0f / _cameraProj[1][1]);
    frameData.isOrthoProjection = _cameraProj[3][3] == 1.0f;
    frameData.focalDistance     = _focalDistance;
    frameData.lensRadius        = _lensRadius;
    frameData.frameIndex        = _frameIndex++;

    // Get the scene size, specifically the maximum distance between any two points in the scene.
    // This is computed as the distance between the min / max corners of the bounding box.
    const Foundation::BoundingBox& bounds = _pScene->bounds();
    frameData.sceneSize                   = glm::length(bounds.max() - bounds.min());

    // Copy the current light buffer for the scene to this frame's light data.
    memcpy(&frameData.lights, &_pScene->lights(), sizeof(frameData.lights));

    int debugMode                = _values.asInt(kLabelDebugMode);
    int traceDepth               = _values.asInt(kLabelTraceDepth);
    traceDepth                   = glm::max(1, glm::min(kMaxTraceDepth, traceDepth));
    frameData.traceDepth         = traceDepth;
    frameData.isDenoisingEnabled = _values.asBoolean(kLabelIsDenoisingEnabled) ? 1 : 0;
    frameData.isForceOpaqueShadowsEnabled =
        _values.asBoolean(kLabelIsForceOpaqueShadowsEnabled) ? 1 : 0;
    frameData.isDiffuseOnlyEnabled   = _values.asBoolean(kLabelIsDiffuseOnlyEnabled) ? 1 : 0;
    frameData.maxLuminance           = _values.asFloat(kLabelMaxLuminance);
    frameData.isDisplayErrorsEnabled = debugMode == kDebugModeErrors ? 1 : 0;

    // If there are no changes compared local CPU copy, then do nothing and return false.
    if (memcmp(&_frameData, &frameData, sizeof(FrameData)) == 0)
        return false; // No changes.

    // Set the local copy of frame data to use for next comparison.
    _frameData = frameData;

    // If staging buffer pointer was passed in, copy to that.
    if (pStaging)
        *pStaging = frameData;

    return true;
}

bool RendererBase::updatePostProcessingGPUStruct(PostProcessing* pStaging)
{
    PostProcessing settings;

    // Compute the scene range, i.e. the near and far distance of the scene bounding box from the
    // current view. Since the view has a direction along the -Z axis, the range is determined as
    // [-maxZ, -minZ].
    Foundation::BoundingBox viewBox = _pScene->bounds().transform(_cameraView);
    vec2 sceneRange(-viewBox.max().z, -viewBox.min().z);

    // Prepare post-processing settings.
    int debugMode                     = _values.asInt(kLabelDebugMode);
    settings.debugMode                = glm::max(0, glm::min(debugMode, kMaxDebugMode));
    settings.isDenoisingEnabled       = _values.asBoolean(kLabelIsDenoisingEnabled) ? 1 : 0;
    settings.isToneMappingEnabled     = _values.asBoolean(kLabelIsToneMappingEnabled);
    settings.isGammaCorrectionEnabled = _values.asBoolean(kLabelIsGammaCorrectionEnabled);
    settings.isAlphaEnabled           = _values.asBoolean(kLabelIsAlphaEnabled);
    settings.brightness               = _values.asFloat3(kLabelBrightness);
    settings.range                    = sceneRange;

    // If there are no changes compared local CPU copy, then do nothing and return false.
    if (memcmp(&_postProcessingData, &settings, sizeof(PostProcessing)) == 0)
        return false; // No changes.

    // Update local CPU copy.
    _postProcessingData = settings;

    // Update staging buffer, if one provided.
    if (pStaging)
        *pStaging = settings;

    return true;
}

bool RendererBase::updateAccumulationGPUStruct(uint32_t sampleIndex, Accumulation* pStaging)
{
    Accumulation settings;
    // Prepare accumulation settings.
    settings.sampleIndex        = sampleIndex;
    settings.isDenoisingEnabled = _values.asBoolean(kLabelIsDenoisingEnabled) ? 1 : 0;

    // If there are no changes compared local CPU copy, then do nothing and return false.
    if (memcmp(&_accumData, &settings, sizeof(Accumulation)) == 0)
        return false; // No changes.

    // Update local CPU copy.
    _accumData = settings;

    // Update staging buffer, if one provided.
    if (pStaging)
        *pStaging = settings;

    return true;
}

// The maximum trace depth for recursion, set on the ray tracing pipeline and not to be exceeded.
const int RendererBase::kMaxTraceDepth = 10;

END_AURORA
