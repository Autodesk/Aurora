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

#include "RendererBase.h"
#include "Resources.h"

BEGIN_AURORA

EnvironmentResource::EnvironmentResource(const Aurora::Path& path, const ResourceMap& container,
    const TypedResourceTracker<EnvironmentResource, IEnvironment>& tracker, IRenderer* pRenderer) :
    ResourceStub(path, container, tracker.tracker()), _pRenderer(pRenderer)
{
    // Initialize the path applicator functions that apply path properties in the environment
    // resource.
    initializePathApplicators({ // Apply the background image (get the associated image resource and
                                // set in environment resource.)
        { Names::EnvironmentProperties::kBackgroundImage,
            [this](string propName, Aurora::Path) {
                IImagePtr pImage = getReferenceResource<ImageResource, IImage>(propName);
                _resource->values().setImage(propName, pImage);
            } },
        // Apply the light image (get the associated image resource and set in environment
        // resource.)
        { Names::EnvironmentProperties::kLightImage, [this](string propName, Aurora::Path) {
             IImagePtr pImage = getReferenceResource<ImageResource, IImage>(propName);
             _resource->values().setImage(propName, pImage);
         } } });

    // Initialize the vec3 applicator functions that apply vec3 properties in the environment
    // resource.
    initializeVec3Applicators({ // Apply the top light color.
        { Names::EnvironmentProperties::kLightTop,
            [this](string propName, glm::vec3 val) {
                _resource->values().setFloat3(propName, (const float*)&val);
            } },
        // Apply the bottom light color.
        { Names::EnvironmentProperties::kLightBottom,
            [this](string propName, glm::vec3 val) {
                _resource->values().setFloat3(propName, (const float*)&val);
            } },
        // Apply the top background color.
        { Names::EnvironmentProperties::kBackgroundTop,
            [this](string propName, glm::vec3 val) {
                _resource->values().setFloat3(propName, (const float*)&val);
            } },
        // Apply the bottom background color.
        { Names::EnvironmentProperties::kBackgroundBottom, [this](string propName, glm::vec3 val) {
             _resource->values().setFloat3(propName, (const float*)&val);
         } } });

    // Initialize the mat4 applicator functions that apply vec3 properties in the environment
    // resource.
    initializeMat4Applicators({ // Apply the background transform matrix.
        { Names::EnvironmentProperties::kBackgroundTransform,
            [this](string propName, glm::mat4 val) {
                _resource->values().setMatrix(propName, (const float*)&val);
            } },
        // Apply the light transform matrix.
        { Names::EnvironmentProperties::kLightTransform, [this](string propName, glm::mat4 val) {
             _resource->values().setMatrix(propName, (const float*)&val);
         } } });

    // Initialize the bool applicator functions that apply bool properties in the environment
    // resource.
    initializeBoolApplicators(
        // Apply the background use screen flag.
        { { Names::EnvironmentProperties::kBackgroundUseScreen, [this](string propName, bool val) {
               _resource->values().setBoolean(propName, val);
           } } });
}

void EnvironmentResource::createResource()
{
    // Create the actual renderer environment resource.
    _resource = _pRenderer->createEnvironmentPointer();
}

MaterialResource::MaterialResource(const Aurora::Path& path, const ResourceMap& container,
    const TypedResourceTracker<MaterialResource, IMaterial>& tracker, IRenderer* pRenderer) :
    ResourceStub(path, container, tracker.tracker()), _pRenderer(pRenderer)
{
    // Setup a default applicator function so all image properties are applied to the underlying
    // material resource.
    initializePathApplicators(
        // material resource.
        { { ResourceStub::kDefaultPropName, [this](string propName, Aurora::Path) {
               // If the material resource's parameter is a sampler, treat path as sampler
               // reference.
               if (_resource->values().type(propName) == IValues::Type::Sampler)
               {
                   ISamplerPtr pSampler = getReferenceResource<SamplerResource, ISampler>(propName);
                   _resource->values().setSampler(propName, pSampler);
               }
               // All other paths are treated as image references.
               else
               {
                   IImagePtr pImage = getReferenceResource<ImageResource, IImage>(propName);
                   _resource->values().setImage(propName, pImage);
               }
           } } });

    // Setup a default applicator function so all int properties are applied to the underlying
    // material resource.
    initializeIntApplicators({ { ResourceStub::kDefaultPropName,
        [this](string propName, int value) { _resource->values().setInt(propName, value); } } });

    // Setup a default applicator function so all bool properties are applied to the underlying
    // material resource.
    initializeBoolApplicators(
        { { ResourceStub::kDefaultPropName, [this](string propName, bool value) {
               _resource->values().setBoolean(propName, value);
           } } });

    // Setup a default applicator function so all bool properties are applied to the underlying
    // material resource.
    initializeFloatApplicators(
        { { ResourceStub::kDefaultPropName, [this](string propName, float value) {
               _resource->values().setFloat(propName, value);
           } } });

    // Setup a default applicator function so all vec3 properties are applied to the underlying
    // material resource.
    initializeVec3Applicators(
        { { ResourceStub::kDefaultPropName, [this](string propName, vec3 value) {
               _resource->values().setFloat3(propName, (float*)&value);
           } } });

    // Setup a default applicator function so all mat4 properties are applied to the underlying
    // material resource.
    initializeMat4Applicators(
        { { ResourceStub::kDefaultPropName, [this](string propName, mat4 value) {
               _resource->values().setMatrix(propName, (float*)&value);
           } } });

    // Setup a default applicator function so all cleared properties are applied to the underlying
    // material resource.
    initializeClearedApplicators({ { ResourceStub::kDefaultPropName,
        [this](string propName) { _resource->values().clearValue(propName); } } });
}

void MaterialResource::createResource()
{
    // Create actual renderer material resource.
    _resource = _pRenderer->createMaterialPointer(_type, _document, path());
}

SamplerResource::SamplerResource(const Aurora::Path& path, const ResourceMap& container,
    const TypedResourceTracker<SamplerResource, ISampler>& tracker, IRenderer* pRenderer) :
    ResourceStub(path, container, tracker.tracker()), _pRenderer(pRenderer)
{
    initializeStringApplicators({
        // All the sampler properties are immutable, so in the applicator functions just invalidate
        // the sampler resource, causing it to be recreated.
        { Names::SamplerProperties::kAddressModeU, [this](string, Aurora::Path) { invalidate(); } },
        { Names::SamplerProperties::kAddressModeV, [this](string, Aurora::Path) { invalidate(); } },
    });
}

void SamplerResource::createResource()
{
    // Create sampler directly from the resource properties.
    _resource = _pRenderer->createSamplerPointer(this->properties());
}

ImageResource::ImageResource(const Aurora::Path& path, const ResourceMap& container,
    const TypedResourceTracker<ImageResource, IImage>& tracker, IRenderer* pRenderer) :
    ResourceStub(path, container, tracker.tracker()), _pRenderer(pRenderer)
{
}

void ImageResource::createResource()
{
    // Ensure descriptor as been set.
    AU_ASSERT(_hasDescriptor, "Can't create image, no descriptor");

    // Setup InitData object for image.
    // TODO: This is legacy struct, replace with descriptor in pointer inteface.
    IImage::InitData initData;
    initData.format        = _descriptor.format;
    initData.width         = _descriptor.width;
    initData.height        = _descriptor.height;
    initData.isEnvironment = _descriptor.isEnvironment;
    initData.linearize     = _descriptor.linearize;
    initData.name          = path();

    // Use the getPixelData callback to get the contents of the image.
    PixelData pixelData;
    _descriptor.getPixelData(pixelData, ivec2(0, 0), ivec2(_descriptor.width, _descriptor.height));
    initData.pImageData = pixelData.address;

    // Create the actual renderer image resource.
    _resource = _pRenderer->createImagePointer(initData);

    // If we have a completion callback, execute that as pixels ahve been passed to renderer and can
    // be deleted.
    if (_descriptor.pixelUpdateComplete)
        _descriptor.pixelUpdateComplete(
            pixelData, ivec2(0, 0), ivec2(_descriptor.width, _descriptor.height));
}

void ImageResource::destroyResource()
{
    // TODO: Aurora cannot currently delete an image that is in use, even if synchronized with the
    // renderer.
#if 0
    RendererBase* pBaseRenderer = (RendererBase*)_pRenderer;
    pBaseRenderer->destroyImage(_resource);
#endif
}

GeometryResource::GeometryResource(const Aurora::Path& path, const ResourceMap& container,
    const TypedResourceTracker<GeometryResource, IGeometry>& tracker, IRenderer* pRenderer) :
    ResourceStub(path, container, tracker.tracker()), _pRenderer(pRenderer)
{
}

void GeometryResource::createResource()
{
    AU_ASSERT(_hasDescriptor, "No descriptor, can't create geometry");
    _resource = _pRenderer->createGeometryPointer(_descriptor, path());
}

InstanceResource::InstanceResource(const Path& path, const ResourceMap& container,
    const TypedResourceTracker<InstanceResource, IInstance>& tracker, IScene* pScene) :
    ResourceStub(path, container, tracker.tracker()), _pScene(pScene)
{

    // Initialize the default properties of an instance resource.
    initializeProperties({ { Names::InstanceProperties::kMaterialLayers, vector<string>({}) },
        { Names::InstanceProperties::kGeometryLayers, vector<string>({}) },
        { Names::InstanceProperties::kMaterial, "" }, { Names::InstanceProperties::kVisible, true },
        { Names::InstanceProperties::kTransform, mat4() } });

    // Initialize the path applicator functions that apply path properties in the instance
    // resource.
    initializePathApplicators(
        { // Apply the geometry property. This is treated as a property internally, but will trigger
          // resource invalidation if changed (i.e. will force recreation of the resource)
            { Names::InstanceProperties::kGeometry,
                [this](string propName, Aurora::Path) {
                    IGeometryPtr pGeom =
                        getReferenceResource<GeometryResource, IGeometry>(propName);
                    if (_resource->geometry() != pGeom)
                        invalidate();
                } },
            // Apply the material property to the instance.
            { Names::InstanceProperties::kMaterial, [this](string propName, Aurora::Path) {
                 IMaterialPtr pMtl = getReferenceResource<MaterialResource, IMaterial>(propName);
                 _resource->setMaterial(pMtl);
             } } });

    initializePathArrayApplicators({ { Names::InstanceProperties::kMaterialLayers,
                                         [this](string, vector<Aurora::Path>) { invalidate(); } },
        { Names::InstanceProperties::kGeometryLayers,
            [this](string, vector<Aurora::Path>) { invalidate(); } } });

    // Initialize the mat4 applicator functions that apply mat4 properties in the instance
    // resource.
    initializeMat4Applicators({ { Names::InstanceProperties::kTransform,
        [this](string, glm::mat4 val) { _resource->setTransform(val); } } });

    // Initialize the int applicator functions that apply int path properties in the instance
    // resource.
    initializeIntApplicators({ { Names::InstanceProperties::kObjectID,
        [this](string, int val) { _resource->setObjectIdentifier(val); } } });

    // Initialize the bool applicator functions that apply bpp path properties in the instance
    // resource.
    initializeBoolApplicators({ { Names::InstanceProperties::kVisible,
        [this](string, bool val) { _resource->setVisible(val); } } });
}

void InstanceResource::createResource()
{
    // Get the referenced material resource.
    IMaterialPtr pMaterial =
        getReferenceResource<MaterialResource, IMaterial>(Names::InstanceProperties::kMaterial);

    // Get the referenced geometry resource.
    IGeometryPtr pGeometry =
        getReferenceResource<GeometryResource, IGeometry>(Names::InstanceProperties::kGeometry);

    vector<IMaterialPtr> materialLayers = getReferenceResources<MaterialResource, IMaterial>(
        Names::InstanceProperties::kMaterialLayers);
    vector<IGeometryPtr> geometryLayers = getReferenceResources<GeometryResource, IGeometry>(
        Names::InstanceProperties::kGeometryLayers);

    vector<LayerDefinition> materialLayerDefs;
    for (size_t i = 0; i < materialLayers.size(); i++)
    {
        IGeometryPtr pGeom = nullptr;
        if (i < geometryLayers.size())
            pGeom = geometryLayers[i];
        materialLayerDefs.push_back(make_pair(materialLayers[i], pGeom));
    }

    // Get the transform matrix.
    const mat4 mtx = properties()[Names::InstanceProperties::kTransform].asMatrix4();

    // Create instance.
    _resource = _pScene->addInstancePointer(path(), pGeometry, pMaterial, mtx, materialLayerDefs);
}

void InstanceResource::destroyResource()
{
    // Destroy the instance resource.
    _resource.reset();
}

END_AURORA
