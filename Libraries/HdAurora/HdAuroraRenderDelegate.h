// Copyright 2024 Autodesk, Inc.
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

#include <pxr/imaging/hgi/hgi.h>

class GroundPlane;
class HdAuroraRenderPass;
class HdAuroraRenderBuffer;
class HdAuroraImageCache;

// Function used to update a Hydra render setting.
using UpdateRenderSettingFunction = function<bool(VtValue const& value)>;

class HdAuroraRenderDelegate final : public HdRenderDelegate
{
public:
    HdAuroraRenderDelegate(HdRenderSettingsMap const& settings = {});
    ~HdAuroraRenderDelegate() override;

    void SetDrivers(HdDriverVector const& drivers) override;

    const TfTokenVector& GetSupportedRprimTypes() const override;
    const TfTokenVector& GetSupportedSprimTypes() const override;
    const TfTokenVector& GetSupportedBprimTypes() const override;

    HdRprim* CreateRprim(TfToken const& typeId, SdfPath const& rprimId) override;
    HdSprim* CreateSprim(TfToken const& typeId, SdfPath const& sprimId) override;
    HdBprim* CreateBprim(TfToken const& typeId, SdfPath const& bprimId) override;

    void DestroyRprim(HdRprim* rPrim) override;
    void DestroySprim(HdSprim* sprim) override;
    void DestroyBprim(HdBprim* bprim) override;

    HdSprim* CreateFallbackSprim(TfToken const& typeId) override;
    HdBprim* CreateFallbackBprim(TfToken const& typeId) override;

    HdInstancer* CreateInstancer(HdSceneDelegate* delegate, SdfPath const& id) override;
    void DestroyInstancer(HdInstancer* instancer) override;

    HdRenderParam* GetRenderParam() const override;

    HdAovDescriptor GetDefaultAovDescriptor(TfToken const& name) const override;

    HdRenderPassSharedPtr CreateRenderPass(
        HdRenderIndex* index, HdRprimCollection const& collection) override;

    HdResourceRegistrySharedPtr GetResourceRegistry() const override;
    void CommitResources(HdChangeTracker* tracker) override;
    void SetRenderSetting(TfToken const& key, VtValue const& value) override;
    VtValue GetRenderSetting(TfToken const& key) const override;

    Aurora::IRendererPtr GetRenderer() { return _auroraRenderer; }
    Aurora::IScenePtr GetScene() { return _auroraScene; }

    // Update the scene bounds with the AABB for a single mesh.
    void UpdateBounds(const GfVec3f& min, const GfVec3f& max);
    // Reset the scene bounds.
    // This will reset them to invalid values, so the next call to UpdateBounds will set the bounds.
    void ResetBounds();
    // Are the bounds currently valid?
    bool BoundsValid() { return _boundsValid; }

    Aurora::Foundation::SampleCounter& GetSampleCounter() { return _sampleCounter; }
    bool SampleRestartNeeded() const { return _sampleRestartNeeded; }
    void SetSampleRestartNeeded(bool needed) { _sampleRestartNeeded = needed; }

    void ActivateRenderPass(
        HdAuroraRenderPass* pRenderPass, const map<TfToken, HdAuroraRenderBuffer*>& pRenderBuffers);
    void RenderPassDestroyed(HdAuroraRenderPass* pRenderPass);

    // Returns Hydra graphics interface
    Hgi* GetHgi() { return _hgi; }

    // Settings for shared buffers.
    // NOTE: This determines whether the GetRawResource() function of the HgiTexture object created
    // in HdAuroraRenderBuffer::Resolve() returns a handle to shared GPU-resident object.
    enum class RenderBufferSharingType
    {
        NONE,
        DIRECTX_TEXTURE_HANDLE,
        OPENGL_TEXTURE_ID
    };

    RenderBufferSharingType GetRenderBufferSharingType() { return _renderBufferSharingType; }
    mutex& rendererMutex() { return _rendererMutex; }
    // Get the primIndex mutex, required to access prim. index via GetSprimSubtree as this is not
    // thread safe.
    mutex& primIndexMutex() { return _primIndexMutex; }
    HdAuroraImageCache& imageCache() { return *_pImageCache; }

    void setAuroraEnvironmentLightImagePath(const Aurora::Path& path)
    {
        _auroraEnvironmentLightImagePath = path;
        _bEnvironmentIsDirty             = true;
    }

    void setAuroraEnvironmentLightTransform(const glm::mat4& xform)
    {
        _auroraEnvironmentLightTransform = xform;
        _bEnvironmentIsDirty             = true;
    }

    void UpdateAuroraEnvironment();

private:
    mutex _rendererMutex;
    mutex _primIndexMutex;

    map<TfToken, UpdateRenderSettingFunction> _settingFunctions;
    Aurora::IRendererPtr _auroraRenderer;
    Aurora::IScenePtr _auroraScene;
    unique_ptr<GroundPlane> _pGroundPlane;
    unique_ptr<HdAuroraImageCache> _pImageCache;

    HdAuroraRenderPass* _activeRenderPass = nullptr;

    bool _sampleRestartNeeded = true;
    Aurora::Foundation::SampleCounter _sampleCounter;

    GfVec3f _boundsMin = GfVec3f(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    GfVec3f _boundsMax = GfVec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool _boundsValid  = false;

    // Settings for background.
    bool _bEnvironmentIsDirty             = true;
    bool _alphaEnabled                    = false;
    bool _useEnvironmentLightAsBackground = true;
    GfVec3f _backgroundTopColor           = GfVec3f(1.0f, 1.0f, 1.0f);
    GfVec3f _backgroundBottomColor        = GfVec3f(1.0f, 1.0f, 1.0f);
    glm::mat4 _auroraEnvironmentLightTransform;
    string _backgroundImageFilePath;
    Aurora::Path _auroraEnvironmentBackgroundImagePath;
    Aurora::Path _auroraEnvironmentLightImagePath;
    Aurora::Path _auroraEnvironmentPath;

    RenderBufferSharingType _renderBufferSharingType = RenderBufferSharingType::NONE;
    Hgi* _hgi;
};

// A class that manages ground plane data, including an Aurora ground plane object.
class GroundPlane
{
public:
    using ValueType = tuple<bool, GfVec3f, GfVec3f, float, GfVec3f, float, GfVec3f, float>;
    GroundPlane(Aurora::IRenderer* pRenderer, Aurora::IScene* pScene);
    bool update(const ValueType& value);

private:
    Aurora::IGroundPlanePtr _pGroundPlane;
    ValueType _value;
};
