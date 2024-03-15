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
#include "Resources.h"
#include "SceneBase.h"

BEGIN_AURORA

// Resource paths for default resources.
Path SceneBase::kDefaultEnvironmentName = "__AuroraDefaultEnvironment";
Path SceneBase::kDefaultMaterialName    = "__AuroraDefaultMaterial";
Path SceneBase::kDefaultGeometryName    = "__AuroraDefaultInstance";
Path SceneBase::kDefaultInstanceName    = "__AuroraDefaultGeometry";
Path SceneBase::kDefaultImageName       = "__AuroraDefaultImage";

RendererBase* SceneBase::rendererBase()
{
    return static_cast<RendererBase*>(_pRenderer);
}

void SceneBase::createDefaultResources()
{
    if (_resources.find(kDefaultEnvironmentName) != _resources.end())
        _resources.erase(kDefaultEnvironmentName);
    if (_resources.find(kDefaultInstanceName) != _resources.end())
        _resources.erase(kDefaultInstanceName);
    if (_resources.find(kDefaultImageName) != _resources.end())
        _resources.erase(kDefaultImageName);

    // Create a default environment resource, and set it as current environment.
    _pDefaultEnvironmentResource = make_shared<EnvironmentResource>(
        kDefaultEnvironmentName, _resources, _environments, _pRenderer);
    _resources[kDefaultEnvironmentName] = _pDefaultEnvironmentResource;
    setEnvironment("");

    _pErrorImageData                     = make_shared<ImageAsset>();
    _pErrorImageData->data.format        = ImageFormat::Integer_RGBA;
    _pErrorImageData->data.width         = 1;
    _pErrorImageData->data.height        = 1;
    _pErrorImageData->data.isEnvironment = false;
    _pErrorImageData->data.linearize     = false;
    _pErrorImageData->pixels             = make_unique<unsigned char[]>(4);
    _pErrorImageData->data.pImageData    = _pErrorImageData->pixels.get();
    _pErrorImageData->pixels[0]          = 0xff;
    _pErrorImageData->pixels[1]          = 0xff;
    _pErrorImageData->pixels[2]          = 0xff;
    _pErrorImageData->pixels[3]          = 0xff;

    // Create default material resource, and make it permanent, so underlying GPU material always
    // available.
    _pDefaultMaterialResource =
        make_shared<MaterialResource>(kDefaultMaterialName, _resources, _materials, _pRenderer);
    _resources[kDefaultMaterialName] = _pDefaultMaterialResource;
    _pDefaultMaterialResource->incrementPermanentRefCount();

    GeometryDescriptor geomDesc;
    geomDesc.type                                                      = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kPosition] = AttributeFormat::Float3;
    geomDesc.vertexDesc.count = _defaultGeometryVerts.size() / 3;
    geomDesc.indexCount       = _defaultGeometryIndices.size();
    geomDesc.getAttributeData = [this](AttributeDataMap& buffers, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == _defaultGeometryVerts.size() / 3, "Partial update not supported");

        buffers[Names::VertexAttributes::kPosition].address = _defaultGeometryVerts.data();
        buffers[Names::VertexAttributes::kPosition].size =
            _defaultGeometryVerts.size() * sizeof(vec3);

        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == _defaultGeometryIndices.size(), "Partial update not supported");

        buffers[Names::VertexAttributes::kIndices].address = _defaultGeometryIndices.data();
        buffers[Names::VertexAttributes::kIndices].size =
            _defaultGeometryIndices.size() * sizeof(uint32_t);
        buffers[Names::VertexAttributes::kIndices].stride = sizeof(uint32_t);

        return true;
    };

    setGeometryDescriptor(kDefaultGeometryName, geomDesc);

    _pDefaultInstanceResource =
        make_shared<InstanceResource>(kDefaultInstanceName, _resources, _instances, this);
    _pDefaultInstanceResource->setProperties(
        { { Names::InstanceProperties::kGeometry, kDefaultGeometryName } });
    _resources[kDefaultInstanceName] = _pDefaultInstanceResource;

    ImageDescriptor imageDesc;
    imageDesc.isEnvironment = false;
    imageDesc.linearize     = true;
    imageDesc.getData       = [this](ImageData& dataOut, AllocateBufferFunction alloc) {
        dataOut.pPixelBuffer = _defaultImagePixels.data();
        dataOut.bufferSize   = _defaultImagePixels.size();
        dataOut.dimensions   = { 2, 2 };
        dataOut.format       = ImageFormat::Integer_RGBA;
        return true;
    };

    _pDefaultImageResource =
        make_shared<ImageResource>(kDefaultImageName, _resources, _images, _pRenderer);
    _pDefaultImageResource->setDescriptor(imageDesc);
    _resources[kDefaultImageName] = _pDefaultImageResource;
    _pDefaultImageResource->incrementPermanentRefCount();
}

SceneBase::~SceneBase()
{
    _environments.shutdown();
    _instances.shutdown();
    _geometry.shutdown();
    _environments.shutdown();
    _images.shutdown();
    _samplers.shutdown();
    _materials.shutdown();
}
void SceneBase::setBounds(const vec3& min, const vec3& max)
{
    _bounds.reset();
    _bounds.add(min);
    _bounds.add(max);

    assert(_bounds.isValid());
}

void SceneBase::setBounds(const float* min, const float* max)
{
    _bounds.reset();
    _bounds.add(make_vec3(min));
    _bounds.add(make_vec3(max));

    assert(_bounds.isValid());
}

void SceneBase::addPermanent(const Path& path)
{
    _resources[path]->incrementPermanentRefCount();
}

void SceneBase::removePermanent(const Path& path)
{
    _resources[path]->decrementPermanentRefCount();
}

void SceneBase::setSamplerProperties(const Path& atPath, const Properties& props)
{
    // Get sampler resource.
    auto pSamplerRes = getResource<SamplerResource>(atPath);

    // If no existing resource create one.
    if (!pSamplerRes)
    {
        pSamplerRes = make_shared<SamplerResource>(atPath, _resources, _samplers, _pRenderer);
        _resources[atPath] = pSamplerRes;
    }

    // Set the sampler .
    pSamplerRes->setProperties(props);
}

void SceneBase::setImageDescriptor(const Path& atPath, const ImageDescriptor& desc)
{
    // Get image resource.
    auto pImageRes = getResource<ImageResource>(atPath);

    // If no existing resource create one.
    if (!pImageRes)
    {
        pImageRes          = make_shared<ImageResource>(atPath, _resources, _images, _pRenderer);
        _resources[atPath] = pImageRes;
    }

    // Set the image descriptor.
    pImageRes->setDescriptor(desc);
}

ResourceType SceneBase::getResourceType(const Path& path)
{
    auto iter = _resources.find(path);
    if (iter == _resources.end())
    {
        return ResourceType::Invalid;
    }

    return iter->second->type();
}

void SceneBase::setImageFromFilePath(
    const Path& atPath, const string& filePath, bool forceLinear, bool isEnvironment)
{
    // Set file path to Aurora path if empty.
    string imagePath = filePath;
    if (imagePath.empty())
        imagePath = atPath;

    Aurora::ImageDescriptor imageDesc;
    imageDesc.linearize     = forceLinear;
    imageDesc.isEnvironment = isEnvironment;

    // Setup pixel data callback to get address and size from buffer from cache entry.
    string pathToLoad = filePath;
    imageDesc.getData = [this, forceLinear, pathToLoad](
                            Aurora::ImageData& dataOut, AllocateBufferFunction /* alloc*/) {
        shared_ptr<ImageAsset> pImageAsset;

        // Lookup in loaded images map, return
        auto iter = _loadedImages.find(pathToLoad);
        if (iter != _loadedImages.end())
        {
            pImageAsset = iter->second;
        }
        else
        {
            pImageAsset = rendererBase()->assetManager()->acquireImage(pathToLoad);

            if (!pImageAsset)
            {
                pImageAsset = _pErrorImageData;
                AU_ERROR("Failed to load image %s", pathToLoad.c_str());
            }
            _loadedImages[pathToLoad] = pImageAsset;
        }

        // Fill in output data.
        dataOut.format       = pImageAsset->data.format;
        dataOut.dimensions   = { pImageAsset->data.width, pImageAsset->data.height };
        dataOut.pPixelBuffer = pImageAsset->pixels.get();
        dataOut.bufferSize   = pImageAsset->sizeBytes;

        // Override the linearize value with the one inferred from the image file, unless client
        // passed forceLinear flag.
        dataOut.overrideLinearize = !forceLinear;
        dataOut.linearize         = pImageAsset->data.linearize;

        return true;
    };
    imageDesc.updateComplete = [this, pathToLoad]() { _loadedImages.erase(pathToLoad); };

    setImageDescriptor(atPath, imageDesc);
}

void SceneBase::setMaterialType(
    const Path& atPath, const std::string& materialType, const std::string& document)
{
    // Get material resource.
    auto pMaterialRes = getResource<MaterialResource>(atPath);

    // If no existing resource create one.
    if (!pMaterialRes)
    {
        pMaterialRes = make_shared<MaterialResource>(atPath, _resources, _materials, _pRenderer);
        _resources[atPath] = pMaterialRes;
    }

    // Set the material type.
    pMaterialRes->setType(materialType, document);
}

Paths SceneBase::addInstances(const Path& geometry, const InstanceDefinitions& definitions)
{
    // Returned paths vectors.
    Paths paths;

    // Iterate through all the instance definitions.
    for (size_t i = 0; i < definitions.size(); i++)
    {
        // Add path to return value.
        paths.push_back(definitions[i].path);

        // Add the instance to the scene.
        addInstance(definitions[i].path, geometry, definitions[i].properties);
    }

    return paths;
}

bool SceneBase::setEnvironmentProperties(const Path& atPath, const Properties& properties)
{
    // Get environment resource.
    auto pEnvRes = getResource<EnvironmentResource>(atPath);

    // If no existing resource create one.
    if (!pEnvRes)
    {
        pEnvRes = make_shared<EnvironmentResource>(atPath, _resources, _environments, _pRenderer);
        _resources[atPath] = pEnvRes;
    }

    // Set environment properties.
    pEnvRes->setProperties(properties);

    return true;
}

bool SceneBase::setEnvironment(const Path& environment)
{
    // If this is just empty path, set to the default environment.
    if (environment.empty())
    {
        return setEnvironment(kDefaultEnvironmentName);
    }

    // If we already have an environment decrement it's permanent ref count (as it's no longer
    // attached to the scene).
    if (_pEnvironmentResource)
        _pEnvironmentResource->decrementPermanentRefCount();

    // Get environment resource stub.
    _pEnvironmentResource = getResource<EnvironmentResource>(environment);

    // Fail if no environment exists.
    AU_ASSERT(_pEnvironmentResource, "Environment not found at path %s", environment.c_str());

    // Increment permanent reference count (as environment is attached to the scene.)
    _pEnvironmentResource->incrementPermanentRefCount();

    return true;
}

void SceneBase::setMaterialProperties(const Path& atPath, const Properties& materialProperties)
{
    // Get material resource
    auto pMaterialRes = getResource<MaterialResource>(atPath);

    // Create resource if it doesn't exist yet.
    if (!pMaterialRes)
    {
        pMaterialRes = make_shared<MaterialResource>(atPath, _resources, _materials, _pRenderer);
        _resources[atPath] = pMaterialRes;
    }

    // Set properties.
    pMaterialRes->setProperties(materialProperties);
}

void SceneBase::setGeometryDescriptor(const Path& atPath, const GeometryDescriptor& desc)
{
    // Look for existing geometry resource.
    auto pGeom = getResource<GeometryResource>(atPath);

    // Create one if it doesn't exist.
    if (!pGeom)
    {
        pGeom = make_shared<GeometryResource>(
            atPath, _resources, _geometry, reinterpret_cast<IRenderer*>(_pRenderer));
        _resources[atPath] = pGeom;
    }

    // Set descriptor on resource stub (this will trigger resource invalidation if it have an actual
    // geometry pointer)
    pGeom->setDescriptor(desc);
}

bool SceneBase::addInstance(const Path& atPath, const Path& geometry, const Properties& properties)
{
    // Ensure resource does not already exist with this path.
    AU_ASSERT(!isPathValid(atPath),
        "Resource already exists with path %s, can't create instance with that path",
        atPath.c_str());

    // Create new resource stub and put in resource map.
    auto pInstRes      = make_shared<InstanceResource>(atPath, _resources, _instances, this);
    _resources[atPath] = pInstRes;

    // Set geometry (geometry is treated as a special property internally)
    pInstRes->setProperties({ { Names::InstanceProperties::kGeometry, geometry } });
    // Set the other properties.
    pInstRes->setProperties(properties);

    // Increment the permanent ref counts, as instances are activated as soon as they are added to
    // the scene. This will create resources for this instance and any resources it references
    // directly or indirectly (geometry, material, images, etc.)
    pInstRes->incrementPermanentRefCount();

    return true;
}

void SceneBase::setInstanceProperties(const Path& instance, const Properties& instanceProperties)
{
    _resources[instance]->setProperties(instanceProperties);
}

void SceneBase::removeInstance(const Path& path)
{
    _resources[path]->decrementPermanentRefCount();
    _resources.erase(path);
}

void SceneBase::removeInstances(const Paths& paths)
{
    // Remove all the paths.
    for (const Path& path : paths)
    {
        removeInstance(path);
    }
}

void SceneBase::setInstanceProperties(const Paths& paths, const Properties& instanceProperties)
{

    // Process all paths/instances.
    for (const Path& path : paths)
    {
        _resources[path]->setProperties(instanceProperties);
    }
}

void SceneBase::preUpdate()
{
    // Create a default instance so the scene is not completely empty.
    if (_instances.activeCount() == 0)
    {
        if (!_pDefaultInstanceResource->isActive())
            _pDefaultInstanceResource->incrementPermanentRefCount();
    }
    else
    {
        if (_pDefaultInstanceResource->isActive())
            _pDefaultInstanceResource->decrementPermanentRefCount();
    }
}

void SceneBase::update()
{
    // Update all the active resources, this will build the ResourceNotifier that stores the set of
    // active resources for this frame.
    _instances.update();
    _geometry.update();
    _environments.update();
    _materials.update();
    _samplers.update();
    _images.update();
}

bool SceneBase::isPathValid(const Path& path)
{
    return _resources.find(path) != _resources.end();
}

END_AURORA
