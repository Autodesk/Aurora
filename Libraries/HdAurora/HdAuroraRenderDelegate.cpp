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

#pragma warning(push)
#pragma warning(disable : 4003)
#include <pxr/imaging/hgi/tokens.h>
#pragma warning(pop)

#include "HdAuroraCamera.h"
#include "HdAuroraImageCache.h"
#include "HdAuroraInstancer.h"
#include "HdAuroraLight.h"
#include "HdAuroraMaterial.h"
#include "HdAuroraMesh.h"
#include "HdAuroraRenderBuffer.h"
#include "HdAuroraRenderDelegate.h"
#include "HdAuroraRenderPass.h"
#include "HdAuroraTokens.h"

const TfTokenVector SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
};

const TfTokenVector SUPPORTED_SPRIM_TYPES = {
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->distantLight,
};

const TfTokenVector SUPPORTED_BPRIM_TYPES = {
    HdPrimTypeTokens
        ->renderBuffer // Supporting renderBuffer BPrim tells Hydra to create AOVs for us
};

const TfTokenVector& HdAuroraRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector& HdAuroraRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

const TfTokenVector& HdAuroraRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdAovDescriptor HdAuroraRenderDelegate::GetDefaultAovDescriptor(TfToken const& name) const
{
    if (name == HdAovTokens->color)
    {
        return HdAovDescriptor(HdFormatFloat16Vec4, false, VtValue(GfVec4f(0.0f)));
    }
    else if (name == HdAovTokens->depth)
    {
        return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
    }

    return HdAovDescriptor(HdFormatInvalid, false, VtValue());
}

static std::mutex mutexResourceRegistry;
static int counterResourceRegistry = 0;
static HdResourceRegistrySharedPtr resourceRegistry;

HdAuroraRenderDelegate::HdAuroraRenderDelegate(HdRenderSettingsMap const& settings) :
    HdRenderDelegate(settings),
    _auroraRenderer(Aurora::createRenderer()),
    _sampleCounter(33, 250, 50),
    _hgi(nullptr)
{
    if (_auroraRenderer)
    {
        // TODO: For long-term, we need an API to set material unit information from client
        // side.
        // Background: "1 ASM unit = 1 cm" in Inventor! Unit section of ASM tutorial mentions
        // the words we want confirm: "ASM is unit-less so it's up to the calling application to
        // decide what one unit of ASM represents. Inventor has chosen: 1 ASM unit = 1 cm.
        // Inventor allows the user to select mm or inches but all Inventor/ASM transaction is
        // done in cm." That is, for Inventor Alpha release, it seems easier to use centimeter
        // for Aurora for now.
        _auroraRenderer->options().setString("units", "centimeter");

        // Turn gamma correction off.  Leave tonemapping and color space correction to Hydra or
        // the application.
        _auroraRenderer->options().setBoolean("isGammaCorrectionEnabled", false);

        // These render settings are the same as the default values.
        // Convenient to have them here to understand the capabilities and options of the
        // renderer.
        float intensity[3] = { 1.0f, 1.0f, 1.0f };
        _auroraRenderer->options().setFloat3("brightness", intensity);
        _auroraRenderer->options().setBoolean("isToneMappingEnabled", false);
        _auroraRenderer->options().setFloat("maxLuminance", 1000.0f);
        _auroraRenderer->options().setBoolean("isDiffuseOnlyEnabled", false);
        _auroraRenderer->options().setInt("traceDepth", 5);
        _auroraRenderer->options().setBoolean("isDenoisingEnabled", false);
        _auroraRenderer->options().setBoolean("alphaEnabled", false);
        _sampleCounter.setMaxSamples(1000);
        _sampleCounter.reset();

        // create a new scene for this renderer.
        _auroraScene = _auroraRenderer->createScene();
        _pImageCache = std::make_unique<HdAuroraImageCache>(_auroraScene);

        // Set default directional light off
        float color[3]       = { 0.0f, 0.0f, 0.0f };
        float dir[3]         = { 0.0f, 0.0f, 1.0f };
        float lightIntensity = 0.0f;
        _auroraScene->setLight(lightIntensity, color, dir);

        // Create a ground plane object, which is assigned to the scene.
        _pGroundPlane = make_unique<GroundPlane>(_auroraRenderer.get(), _auroraScene.get());

        // add the scene to the renderer.
        _auroraRenderer->setScene(_auroraScene);

        // Create an aurora path based on Hydra id.
        _auroraEnvironmentPath = "HdAuroraEnvironment";

        // Create an environment for path by setting empty properties.s
        _auroraScene->setEnvironmentProperties(_auroraEnvironmentPath, {});

        // Set the scene's environment.
        _auroraScene->setEnvironment(_auroraEnvironmentPath);

        // Set up the setting functions, that are called when render settings change.
        _settingFunctions[HdAuroraTokens::kTraceDepth] = [this](VtValue const& value) {
            _auroraRenderer->options().setInt("traceDepth", value.Get<int>());
            return true;
        };
        _settingFunctions[HdAuroraTokens::kMaxSamples] = [this](VtValue const& value) {
            int currentSamples = static_cast<int>(_sampleCounter.currentSamples());
            // Newly max sample value is smaller than current sample, stop render and keep the
            // current number, otherwise set to the new value.
            _sampleCounter.setMaxSamples(std::max(currentSamples, value.Get<int>()));
            return false;
        };
        _settingFunctions[HdAuroraTokens::kIsDenoisingEnabled] = [this](VtValue const& value) {
            _auroraRenderer->options().setBoolean("isDenoisingEnabled", value.Get<bool>());
            return true;
        };
        _settingFunctions[HdAuroraTokens::kIsAlphaEnabled] = [this](VtValue const& value) {
            _auroraRenderer->options().setBoolean("alphaEnabled", value.Get<bool>());
            _bEnvironmentIsDirty = true;
            return false;
        };
        _settingFunctions[HdAuroraTokens::kUseEnvironmentImageAsBackground] =
            [this](VtValue const& value) {
                _useEnvironmentLightAsBackground = value.Get<bool>();
                _bEnvironmentIsDirty             = true;
                return false;
            };
        _settingFunctions[HdAuroraTokens::kBackgroundImage] = [this](VtValue const& value) {
            _backgroundImageFilePath = value.Get<SdfAssetPath>().GetAssetPath();
            _bEnvironmentIsDirty     = true;
            return false;
        };
        _settingFunctions[HdAuroraTokens::kBackgroundColors] = [this](VtValue const& value) {
            const auto& colors = value.Get<VtVec3fArray>();
            if (colors.size() == 2 &&
                (_backgroundTopColor != colors[0] || _backgroundBottomColor != colors[1]))
            {
                _backgroundTopColor    = colors[0];
                _backgroundBottomColor = colors[1];
                _bEnvironmentIsDirty   = true;
            }
            return false;
        };
        _settingFunctions[HdAuroraTokens::kGroundPlaneSettings] = [this](VtValue const& value) {
            // Update the ground plane object, which return true if the data has changed.
            return _pGroundPlane->update(value.Get<GroundPlane::ValueType>());
        };
        _settingFunctions[HdAuroraTokens::kIsSharedHandleEnabled] = [this](VtValue const& value) {
            // Aurora provides shared DX or GL texture handles
            if (value.Get<bool>())
            {
                _renderBufferSharingType = RenderBufferSharingType::DIRECTX_TEXTURE_HANDLE;
            }
            else
                _renderBufferSharingType = RenderBufferSharingType::NONE;

            // Set render settings that can be queried by the scene delegate
            switch (_renderBufferSharingType)
            {
            case RenderBufferSharingType::NONE:
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleDirectX, VtValue(false));
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleOpenGL, VtValue(false));
                break;
            case RenderBufferSharingType::DIRECTX_TEXTURE_HANDLE:
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleDirectX, VtValue(true));
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleOpenGL, VtValue(false));
                break;
            case RenderBufferSharingType::OPENGL_TEXTURE_ID:
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleDirectX, VtValue(false));
                HdRenderDelegate::SetRenderSetting(
                    HdAuroraTokens::kIsSharedHandleOpenGL, VtValue(true));
                break;
            }
            return true;
        };
    }
    else
    {
        TF_FATAL_ERROR("HdAurora fails to create renderer!");
    }
}

void HdAuroraRenderDelegate::SetDrivers(HdDriverVector const& drivers)
{
    if (_hgi)
    {
        TF_CODING_ERROR("Cannot set HdDriver twice for a render delegate.");
        return;
    }

    // For HdAurora we want to use the Hgi driver, so extract it.
    for (HdDriver* hdDriver : drivers)
    {
        if (hdDriver->name == pxr::HgiTokens->renderDriver &&
            hdDriver->driver.IsHolding<pxr::Hgi*>())
        {
            _hgi = hdDriver->driver.UncheckedGet<pxr::Hgi*>();
            break;
        }
    }

    std::lock_guard<std::mutex> guard(mutexResourceRegistry);

    if (counterResourceRegistry++ == 0)
    {
        resourceRegistry.reset(new HdResourceRegistry());
    }

    TF_VERIFY(_hgi, "HdAurora requires a Hgi HdDriver.");
}

HdAuroraRenderDelegate::~HdAuroraRenderDelegate()
{
    std::lock_guard<std::mutex> guard(mutexResourceRegistry);
    if (counterResourceRegistry-- == 1)
    {
        resourceRegistry.reset();
    }
}

HdResourceRegistrySharedPtr HdAuroraRenderDelegate::GetResourceRegistry() const
{
    return resourceRegistry;
}

void HdAuroraRenderDelegate::CommitResources(HdChangeTracker* /* tracker */)
{
    resourceRegistry->Commit();
}

void HdAuroraRenderDelegate::SetRenderSetting(TfToken const& key, VtValue const& value)
{
    // Get settings version before applying new value.
    unsigned int prevVersion = _settingsVersion;

    // first, call the parent method to actually set the value
    HdRenderDelegate::SetRenderSetting(key, value);

    // Is a sample restart needed based on settings change?
    bool restartNeeded = false;

    // kIsRestartEnabled is a special key, it is used to restart the sampling.
    // It triggers a restart even if the value is alread set to true.
    if (HdAuroraTokens::kIsRestartEnabled == key)
    {
        if (value.Get<bool>())
        {
            restartNeeded = true;
        }
    }

    // If the value changed call the setting function for this key.
    if (prevVersion != _settingsVersion)
    {
        auto iter = _settingFunctions.find(key);
        if (iter != _settingFunctions.end())
        {
            // Get the function from map (if it exists for this key.)
            std::function<bool(VtValue const& value)> func = iter->second;

            // Call function and set restart flag if it returns true.
            if (func(value))
                restartNeeded = true;
        }
    }

    // Set the restart needed flag if any functions returned true.
    _sampleRestartNeeded |= restartNeeded;
}

VtValue HdAuroraRenderDelegate::GetRenderSetting(TfToken const& key) const
{
    if (HdAuroraTokens::kCurrentSamples == key)
    {
        return VtValue(static_cast<int>(_sampleCounter.currentSamples()));
    }
    else if (HdAuroraTokens::kIsAlphaEnabled == key)
    {
        return VtValue(_alphaEnabled);
    }

    return HdRenderDelegate::GetRenderSetting(key);
}

HdRprim* HdAuroraRenderDelegate::CreateRprim(TfToken const& typeId, SdfPath const& rprimId)
{
    if (typeId == HdPrimTypeTokens->mesh)
    {
        return new HdAuroraMesh(rprimId, this);
    }

    return nullptr;
}

void HdAuroraRenderDelegate::DestroyRprim(HdRprim* rPrim)
{
    delete rPrim;
}

HdSprim* HdAuroraRenderDelegate::CreateSprim(TfToken const& typeId, SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdAuroraCamera(sprimId, this);
    }
    else if (typeId == HdPrimTypeTokens->material)
    {
        return new HdAuroraMaterial(sprimId, this);
    }
    else if (typeId == HdPrimTypeTokens->domeLight)
    {
        return new HdAuroraDomeLight(sprimId, this);
    }
    else if (typeId == HdPrimTypeTokens->distantLight)
    {
        return new HdAuroraDistantLight(sprimId, this);
    }
    return nullptr;
}

void HdAuroraRenderDelegate::DestroySprim(HdSprim* sprim)
{
    delete sprim;
}

HdBprim* HdAuroraRenderDelegate::CreateBprim(TfToken const& typeId, SdfPath const& bprimId)
{
    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new HdAuroraRenderBuffer(bprimId, this);
    }

    return nullptr;
}

void HdAuroraRenderDelegate::DestroyBprim(HdBprim* /* bprim */) {}

HdSprim* HdAuroraRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdCamera(SdfPath::EmptyPath());
    }
    else if (typeId == HdPrimTypeTokens->material)
    {
        return new HdAuroraMaterial(SdfPath::EmptyPath(), this);
    }

    return nullptr;
}

HdBprim* HdAuroraRenderDelegate::CreateFallbackBprim(TfToken const& /* typeId */)
{
    return nullptr;
}

HdInstancer* HdAuroraRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, SdfPath const& id)
{
    return new HdAuroraInstancer(delegate, id);
}

void HdAuroraRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
    delete instancer;
}

HdRenderParam* HdAuroraRenderDelegate::GetRenderParam() const
{
    return nullptr;
}

HdRenderPassSharedPtr HdAuroraRenderDelegate::CreateRenderPass(
    HdRenderIndex* index, HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(new HdAuroraRenderPass(index, collection, this));
}

void HdAuroraRenderDelegate::ActivateRenderPass(
    HdAuroraRenderPass* pRenderPass, const std::map<TfToken, HdAuroraRenderBuffer*>& renderBuffers)
{
    // Collect supplied render buffers into target assignments for Aurora.
    Aurora::TargetAssignments targetAssignments;
    auto colorTargetIt = renderBuffers.find(HdAovTokens->color);
    if (colorTargetIt != renderBuffers.end())
        targetAssignments[Aurora::AOV::kFinal] = colorTargetIt->second->GetAuroraBuffer();
    auto depthTargetIt = renderBuffers.find(HdAovTokens->depth);
    if (depthTargetIt != renderBuffers.end())
        targetAssignments[Aurora::AOV::kDepthNDC] = depthTargetIt->second->GetAuroraBuffer();

    // Set the Aurora target assignments.
    _auroraRenderer->setTargets(targetAssignments);

    // Keep track of which render pass is currently active in the Aurora renderer
    _activeRenderPass = pRenderPass;
}

void HdAuroraRenderDelegate::RenderPassDestroyed(HdAuroraRenderPass* pRenderPass)
{
    // Unset the active render pass if needed
    if (_activeRenderPass == pRenderPass)
    {
        _activeRenderPass = nullptr;
    }
}

void HdAuroraRenderDelegate::ResetBounds()
{
    // Reset to min/max floating point values, so next call to up date will replace them.
    _boundsMin   = GfVec3f(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    _boundsMax   = GfVec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    _boundsValid = false;
}

void HdAuroraRenderDelegate::UpdateBounds(const pxr::GfVec3f& min, const GfVec3f& max)
{
    // Update scene bounds for each coordinate.
    for (int j = 0; j < 3; j++)
    {
        _boundsMin[j] = std::min(_boundsMin[j], min[j]);
        _boundsMax[j] = std::max(_boundsMax[j], max[j]);
    }

    // Update the Aurora bounds with new value.
    // TODO: This can happen frequently, but the setBounds() function is known to be simple, so
    // it should be fine in practice. Perhaps use a dirty flag to only update this right before
    // rendering.
    _auroraScene->setBounds((float*)&_boundsMin, (float*)&_boundsMax);

    _boundsValid = true;
}

void HdAuroraRenderDelegate::UpdateAuroraEnvironment()
{
    if (!_bEnvironmentIsDirty)
        return;
    SetSampleRestartNeeded(true);

    if (!_backgroundImageFilePath.empty())
    {
        _auroraEnvironmentBackgroundImagePath =
            imageCache().acquireImage(_backgroundImageFilePath, false, false);
    }
    else
    {
        _auroraEnvironmentBackgroundImagePath.clear();
    }

    // Use the background or light image as background, based on flag.
    const auto& backgroundImage = _useEnvironmentLightAsBackground
        ? _auroraEnvironmentLightImagePath
        : _auroraEnvironmentBackgroundImagePath;

    // Set the image on the environment for background.
    Aurora::Properties props = {
        { Aurora::Names::EnvironmentProperties::kBackgroundTop,
            glm::make_vec3(_backgroundTopColor.data()) },
        { Aurora::Names::EnvironmentProperties::kBackgroundBottom,
            glm::make_vec3(_backgroundBottomColor.data()) },
        { Aurora::Names::EnvironmentProperties::kBackgroundUseScreen,
            !_useEnvironmentLightAsBackground },
        { Aurora::Names::EnvironmentProperties::kBackgroundImage, backgroundImage },
        { Aurora::Names::EnvironmentProperties::kLightImage, _auroraEnvironmentLightImagePath },
    };

    // Issue in the properties code means these need to set seperately.
    props.insert({ Aurora::Names::EnvironmentProperties::kLightTransform,
        _auroraEnvironmentLightTransform });
    props.insert({ Aurora::Names::EnvironmentProperties::kBackgroundTransform,
        _auroraEnvironmentLightTransform });

    GetScene()->setEnvironmentProperties(_auroraEnvironmentPath, props);

    _bEnvironmentIsDirty = false;
}

GroundPlane::GroundPlane(Aurora::IRenderer* pRenderer, Aurora::IScene* pScene)
{
    // Create a ground plane object, set it disabled, and assign it to the scene.
    _pGroundPlane = pRenderer->createGroundPlanePointer();
    _pGroundPlane->values().setBoolean("enabled", false);
    pScene->setGroundPlanePointer(_pGroundPlane);

    // NOTE: _value is initialized with data that may not fully match the (new) Aurora ground
    // plane, but it will be fully initialized on the next call to update(), since the default
    // data won't match any sensible ground plane data.
}

bool GroundPlane::update(const ValueType& value)
{
    // Return false if the value is unchanged.
    if (value == _value)
    {
        return false;
    }

    // Get the ground plane settings from the value.
    auto [isEnabled, position, normal, shadowOpacity, shadowColor, reflectionOpacity,
        reflectionColor, reflectionRoughness] = value;
    _value                                    = value;

    // Apply the settings to the Aurora ground plane object.
    Aurora::IValues& values = _pGroundPlane->values();
    values.setBoolean("enabled", isEnabled);
    values.setFloat3("position", position.data());
    values.setFloat3("normal", normal.data());
    values.setFloat("shadow_opacity", shadowOpacity);
    values.setFloat3("shadow_color", shadowColor.data());
    values.setFloat("reflection_opacity", reflectionOpacity);
    values.setFloat3("reflection_color", reflectionColor.data());
    values.setFloat("reflection_roughness", reflectionRoughness);

    return true;
}
