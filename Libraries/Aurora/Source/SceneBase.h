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

#include "Properties.h"
#include "Resources.h"

BEGIN_AURORA

class EnvironmentResource;
class RendererBase;
struct ImageAsset;

// A base class for implementations of IScene.
class SceneBase : public IScene
{
public:
    // Limits for light counts of different light light types.
    enum LightLimits
    {
        kMaxDistantLights = 4, // Maximum distant lights (must match GPU version in Frame.slang).
    };

    // Structure representing a single distant light.
    // Must match GPU struct in Frame.slang
    struct DistantLight
    {
        // Light color (in RGB) and intensity (in alpha channel.)
        vec4 colorAndIntensity;
        // Direction of light (inverted as expected by shaders.)
        vec3 direction = vec3(0, 0, 1);
        // The light size is converted from a diameter in radians to the cosine of the radius.
        float cosRadius = 0.0f;
    };

    // Structure representing the lights in the scene.
    // Must match GPU struct in Frame.slang
    struct LightData
    {
        // Array of distant lights, only first distantLightCount are used.
        DistantLight distantLights[LightLimits::kMaxDistantLights];

        // Number of active distant lights.
        int distantLightCount = 0;

        // Explicitly pad struct to 16-byte boundary.
        int pad[3];
    };

    SceneBase(IRenderer* pRenderer) : _pRenderer(pRenderer) {}
    ~SceneBase();

    /*** IScene Functions ***/

    ResourceType getResourceType(const Path& path) override;
    void setBounds(const vec3& min, const vec3& max) override;
    void setBounds(const float* min, const float* max) override;
    void setImageDescriptor(const Path& atPath, const ImageDescriptor& desc) override;
    void setImageFromFilePath(
        const Path& atPath, const string& filePath, bool forceLinear, bool isEnvironment) override;
    void setSamplerProperties(const Path& atPath, const Properties& props) override;
    void setMaterialType(
        const Path& atPath, const std::string& materialType, const std::string& document) override;
    Paths addInstances(const Path& geometry, const InstanceDefinitions& definitions) override;
    bool setEnvironmentProperties(
        const Path& environment, const Properties& environmentProperties) override;
    bool setEnvironment(const Path& environment) override;
    void setMaterialProperties(const Path& path, const Properties& materialProperties) override;
    void setInstanceProperties(const Path& path, const Properties& instanceProperties) override;
    void setGeometryDescriptor(const Path& atPath, const GeometryDescriptor& desc) override;

    void addPermanent(const Path& resource) override;
    void removePermanent(const Path& resource) override;

    bool addInstance(
        const Path& atPath, const Path& geometry, const Properties& properties = {}) override;
    void removeInstance(const Path& path) override;
    void removeInstances(const Paths& paths) override;
    void setInstanceProperties(const Paths& paths, const Properties& instanceProperties) override;

    /*** Functions ***/

    void update();
    void preUpdate();

    shared_ptr<EnvironmentResource> defaultEnvironment();

    const Foundation::BoundingBox& bounds() const { return _bounds; }

    shared_ptr<MaterialResource> defaultMaterialResource() { return _pDefaultMaterialResource; }

    RendererBase* rendererBase();

    LightData& lights() { return _lights; }

protected:
    // Is the path a valid resource path.
    virtual bool isPathValid(const Path& path);

    void createDefaultResources();

    template <typename ResourceType>
    shared_ptr<ResourceType> getResource(const Path& path)
    {
        auto iter = _resources.find(path);
        if (iter == _resources.end())
            return nullptr;
        return dynamic_pointer_cast<ResourceType>(iter->second);
    }

    /*** Protected Variables ***/

    IRenderer* _pRenderer = nullptr;
    Foundation::BoundingBox _bounds;

    LightData _lights;

    ResourceMap _resources;

    // Trackers for all resource types.
    TypedResourceTracker<InstanceResource, IInstance> _instances;
    TypedResourceTracker<GeometryResource, IGeometry> _geometry;
    TypedResourceTracker<EnvironmentResource, IEnvironment> _environments;
    TypedResourceTracker<ImageResource, IImage> _images;
    TypedResourceTracker<SamplerResource, ISampler> _samplers;
    TypedResourceTracker<MaterialResource, IMaterial> _materials;

    shared_ptr<EnvironmentResource> _pEnvironmentResource;
    shared_ptr<EnvironmentResource> _pDefaultEnvironmentResource;
    shared_ptr<MaterialResource> _pDefaultMaterialResource;
    shared_ptr<InstanceResource> _pDefaultInstanceResource;

    // Images loaded with setImageFromFilePath.
    map<string, shared_ptr<ImageAsset>> _loadedImages;

    static Path kDefaultEnvironmentName;
    static Path kDefaultMaterialName;
    static Path kDefaultGeometryName;
    static Path kDefaultInstanceName;
    vector<float> _defaultGeometryVerts = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    vector<int> _defaultGeometryIndices = { 0, 1, 2 };

    shared_ptr<ImageAsset> _pErrorImageData;
};

END_AURORA
