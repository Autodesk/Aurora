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

#include "ResourceStub.h"

BEGIN_AURORA

/// ResourceStub sub-class that implements a renderer material resource.
class MaterialResource : public ResourceStub
{
public:
    MaterialResource(const Aurora::Path& path, const ResourceMap& container,
        const TypedResourceTracker<MaterialResource, IMaterial>& tracker, IRenderer* pRenderer);

    // Call shutdown in the dtor to avoid issues with destructor ordering.
    virtual ~MaterialResource() { shutdown(); }

    const ResourceType& type() override { return resourceType; }

    /// Override the createResource method to implement material creation functionality.
    void createResource() override;

    /// Override the destroyResource method to reset resource pointer and free resource.
    void destroyResource() override { _resource.reset(); }

    /// Get the resource pointer.
    IMaterialPtr resource() const { return _resource; }

    /// Set the material type and document.
    /// If resource is active will make it invalid, forcing recreation of resource.
    /// \param type The Aurora material type string.
    /// \param type The Aurora material document string.
    void setType(const string& type, const string& document)
    {
        // Set type and document.
        _type     = type;
        _document = document;

        // Make resource stub invalid.
        invalidate();
    }

    static constexpr ResourceType resourceType = ResourceType::Material;

private:
    IMaterialPtr _resource;
    IRenderer* _pRenderer;
    string _type     = Names::MaterialTypes::kBuiltIn;
    string _document = "Default";
};

/// ResourceStub sub-class that implements a renderer environment resource.
class EnvironmentResource : public ResourceStub
{
public:
    EnvironmentResource(const Aurora::Path& path, const ResourceMap& container,
        const TypedResourceTracker<EnvironmentResource, IEnvironment>& tracker,
        IRenderer* pRenderer);

    // Call shutdown in the dtor to avoid issues with destructor ordering.
    virtual ~EnvironmentResource() { shutdown(); }

    /// Override the createResource method to implement material creation functionality.
    void createResource() override;

    /// Override the destroyResource method to reset resource pointer and free resource.
    void destroyResource() override { _resource.reset(); }

    const ResourceType& type() override { return resourceType; }

    /// Get the resource pointer.
    IEnvironmentPtr resource() const { return _resource; }

    static constexpr ResourceType resourceType = ResourceType::Environment;

private:
    IEnvironmentPtr _resource;
    IRenderer* _pRenderer;
};

/// ResourceStub sub-class that implements a renderer image resource.
class ImageResource : public ResourceStub
{
public:
    ImageResource(const Aurora::Path& path, const ResourceMap& container,
        const TypedResourceTracker<ImageResource, IImage>& tracker, IRenderer* pRenderer);

    virtual ~ImageResource() { shutdown(); }

    /// Override the createResource method to implement image creation functionality.
    void createResource() override;

    /// Override the destroyResource method to reset resource pointer and free resource.
    void destroyResource() override;

    const ResourceType& type() override { return resourceType; }

    /// Get the resource pointer.
    IImagePtr resource() const { return _resource; }

    /// Set the material type and document.
    /// If resource is active will make it invalid, forcing recreation of resource.
    /// \param descriptor The Aurora image descriptor.
    void setDescriptor(const ImageDescriptor& descriptor)
    {
        _descriptor    = descriptor;
        _hasDescriptor = true;
        invalidate();
    }

    static constexpr ResourceType resourceType = ResourceType::Image;

private:
    IImagePtr _resource;
    IRenderer* _pRenderer;
    ImageDescriptor _descriptor;
    bool _hasDescriptor = false;
    static vector<unsigned int> _defaultImageData;
};

/// ResourceStub sub-class that implements a renderer sampler resource.
class SamplerResource : public ResourceStub
{
public:
    SamplerResource(const Aurora::Path& path, const ResourceMap& container,
        const TypedResourceTracker<SamplerResource, ISampler>& tracker, IRenderer* pRenderer);

    virtual ~SamplerResource() { shutdown(); }

    /// Override the createResource method to implement sampler creation functionality.
    void createResource() override;

    /// Override the destroyResource method to reset resource pointer and free resource.
    void destroyResource() override { _resource.reset(); }

    const ResourceType& type() override { return resourceType; }

    /// Get the resource pointer.
    ISamplerPtr resource() const { return _resource; }

    static constexpr ResourceType resourceType = ResourceType::Sampler;

private:
    ISamplerPtr _resource;
    IRenderer* _pRenderer;
};

/// ResourceStub sub-class that implements a  renderer geometry resource.
class GeometryResource : public ResourceStub
{
public:
    GeometryResource(const Aurora::Path& path, const ResourceMap& container,
        const TypedResourceTracker<GeometryResource, IGeometry>& tracker, IRenderer* pRenderer);

    virtual ~GeometryResource() { shutdown(); }

    void createResource() override;
    void destroyResource() override { _resource.reset(); }

    const ResourceType& type() override { return resourceType; }

    IGeometryPtr resource() const { return _resource; }

    void setDescriptor(const GeometryDescriptor& descriptor)
    {
        _descriptor    = descriptor;
        _hasDescriptor = true;
        invalidate();
    }

    static constexpr ResourceType resourceType = ResourceType::Geometry;

private:
    IGeometryPtr _resource;
    IRenderer* _pRenderer;
    GeometryDescriptor _descriptor;
    bool _hasDescriptor = false;
};

/// ResourceStub sub-class that implements a  renderer instance resource.
class InstanceResource : public ResourceStub
{
public:
    InstanceResource(const Path& path, const ResourceMap& container,
        const TypedResourceTracker<InstanceResource, IInstance>& tracker, IScene* pScene);

    virtual ~InstanceResource()
    {
        // Just reset pointer.  Don't remove from scene (as this can be called from scene dtor when
        // the scene is no longer valid.)
        _resource.reset();
    }

    void createResource() override;
    void destroyResource() override;
    const ResourceType& type() override { return resourceType; }
    IInstancePtr resource() const { return _resource; }

    static constexpr ResourceType resourceType = ResourceType::Instance;

private:
    IInstancePtr _resource;
    IScene* _pScene;
};

END_AURORA
