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
#include "pch.h"

#include "HdAuroraImageCache.h"
#include "HdAuroraLight.h"
#include "HdAuroraRenderDelegate.h"

#include <stdexcept>

#pragma warning(disable : 4506) // inline function warning (from USD but appears in this file)

HdAuroraDomeLight::HdAuroraDomeLight(
    SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate) :
    HdLight(rprimId), _owner(renderDelegate)
{
}

HdAuroraDomeLight::~HdAuroraDomeLight()
{
    _owner->setAuroraEnvironmentLightImagePath("");
}

HdDirtyBits HdAuroraDomeLight::GetInitialDirtyBitsMask() const
{
    return HdLight::DirtyParams;
}

void HdAuroraDomeLight::Sync(
    HdSceneDelegate* delegate, HdRenderParam* /* renderParam */, HdDirtyBits* dirtyBits)
{
    const auto& id = GetId();

    // Get the environment image file path, if any.
    VtValue envFilePathVal = delegate->GetLightParamValue(id, pxr::HdLightTokens->textureFile);
    auto envFilePath       = envFilePathVal.Get<pxr::SdfAssetPath>().GetAssetPath();

    // If the environment image file path has changed, apply the new value.
    if (_environmentImageFilePath != envFilePath)
    {
        // Store the new file path, and reset the history on the renderer for the next frame, as we
        // don't want the prior environment image visible through temporal accumulation.
        _environmentImageFilePath = envFilePath;

        // Attempt to load the environment image, if any.
        if (!_environmentImageFilePath.empty())
        {
            _auroraImagePath = _owner->imageCache().acquireImage(_environmentImageFilePath, true);
        }
        else
        {
            _auroraImagePath.clear();
        }

        // Set this flag to update Aurora background.
        _owner->setAuroraEnvironmentLightImagePath(_auroraImagePath);
    }

    // Set environment and background transform in Aurora environment.
    GfMatrix4d envTransformDbl = delegate->GetTransform(id);
    GfMatrix4f envTransform(envTransformDbl);
    glm::mat4 envMat4 = glm::make_mat4(envTransform.GetTranspose().data());
    if (_envTransform != envMat4)
    {
        _owner->setAuroraEnvironmentLightTransform(envMat4);
        _envTransform = envMat4;
    }

    *dirtyBits &= ~HdLight::AllDirty;
}

HdAuroraDistantLight::HdAuroraDistantLight(
    SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate) :
    HdLight(rprimId), _owner(renderDelegate)
{
    GfVec3f lightDirection     = { 0.1f, -1.0f, 0.1f };
    GfVec3f lightColor         = { 1.0f, 1.0f, 1.0f };
    float lighIntensity        = 0.0f;
    float lightAngularDiameter = 0.1f;
    _owner->GetScene()->setLight(
        lighIntensity, lightColor.data(), lightDirection.data(), lightAngularDiameter);
}

HdAuroraDistantLight::~HdAuroraDistantLight()
{
    // Ensure sampler counter is reset.
    _owner->SetSampleRestartNeeded(true);

    /// Reset the light intensity to zero.
    GfVec3f lightDirection     = { 0.1f, -1.0f, 0.1f };
    GfVec3f lightColor         = { 1.0f, 1.0f, 1.0f };
    float lighIntensity        = 0.0f;
    float lightAngularDiameter = 0.1f;
    _owner->GetScene()->setLight(
        lighIntensity, lightColor.data(), lightDirection.data(), lightAngularDiameter);
}

HdDirtyBits HdAuroraDistantLight::GetInitialDirtyBitsMask() const
{
    return HdLight::DirtyParams;
}

void HdAuroraDistantLight::Sync(
    HdSceneDelegate* delegate, HdRenderParam* /* renderParam */, HdDirtyBits* dirtyBits)
{
    const auto& id = GetId();

    // Ensure sampler counter is reset.
    _owner->SetSampleRestartNeeded(true);

    //// Get the light intensity.
    float intensity = (delegate->GetLightParamValue(id, HdLightTokens->intensity)).Get<float>();

    // Apply exposure.
    float exposure = (delegate->GetLightParamValue(id, HdLightTokens->exposure)).Get<float>();
    intensity *= powf(2.0f, GfClamp(exposure, -50.0f, 50.0f));

    // Normalize the intensity.
    VtValue normalizeVal = delegate->GetLightParamValue(id, HdLightTokens->normalize);
    VtValue angleDegVal  = delegate->GetLightParamValue(id, HdLightTokens->angle);
    if (!normalizeVal.GetWithDefault<bool>(false))
    {
        float area = 1.0f;
        if (angleDegVal.IsHolding<float>())
        {
            // Calculate solid angle area to normalize intensity.
            float angleRadians = angleDegVal.Get<float>() / 180.0f * static_cast<float>(M_PI);
            float solidAngleSteradians =
                2.0f * static_cast<float>(M_PI) * (1.0f - cos(angleRadians / 2.0f));
            area = solidAngleSteradians;
        }
        intensity *= area;
    }

    // Get the light color from Hydra.
    GfVec3f lightColor =
        (delegate->GetLightParamValue(id, HdLightTokens->color)).Get<pxr::GfVec3f>();

    // Compute light direction from transform matrix.
    GfMatrix4f transformMatrix(delegate->GetTransform(id));
    GfVec3f lightDirection = { -transformMatrix[2][0], -transformMatrix[2][1],
        -transformMatrix[2][2] };
    _owner->GetScene()->setLight(intensity, lightColor.data(), lightDirection.data());

    *dirtyBits = Clean;
}
