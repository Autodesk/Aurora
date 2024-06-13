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

#include "HGIScene.h"

#include "CompiledShaders/CommonShaders.h"
#include "CompiledShaders/HGIShaders.h"
#include "HGIImage.h"
#include "HGIMaterial.h"
#include "HGIRenderBuffer.h"
#include "HGIRenderer.h"
#include "Transpiler.h"

using namespace pxr;

BEGIN_AURORA

#define kInvalidTexureIndex -1

HGIInstance::HGIInstance(HGIRenderer* pRenderer, shared_ptr<HGIMaterial> pMaterial,
    shared_ptr<HGIGeometry> pGeom, const mat4& transform) :
    _pGeometry(pGeom), _pMaterial(pMaterial), _pRenderer(pRenderer)
{
    // Set the transform.
    setTransform(transform);
}

HGIScene::HGIScene(HGIRenderer* pRenderer) : SceneBase(pRenderer), _pRenderer(pRenderer)
{
    // Create the transpiler from the map of shader files.
    _transpiler = make_shared<Transpiler>(CommonShaders::g_sDirectory);

    createDefaultResources();
}

bool HGIScene::update()
{
    // Run the base class update (will update common resources)
    SceneBase::update();

    // Update the environment.
    if (_environments.changedThisFrame())
    {
        HGIEnvironmentPtr pEnvironment =
            static_pointer_cast<HGIEnvironment>(_pEnvironmentResource->resource());
        pEnvironment->update();
    }

    // If any active geometry resources have been modified, flush the vertex buffer pool in case
    // there are any pending vertex buffers that are required to update the geometry, and then
    // update the geometry (and update BLAS for "complete" geometry that has position data).
    if (_geometry.changedThisFrame())
    {
        for (HGIGeometry& geom : _geometry.modified().resources<HGIGeometry>())
        {
            geom.update();
        }
    }

    // If any active material resources have been modified update them and build a list of unique
    // samplers for all the active materials.
    if (_materials.changedThisFrame())
    {
        for (HGIMaterial& mtl : _materials.modified().resources<HGIMaterial>())
        {
            mtl.update();
        }
    }

    // Update the acceleration structure if any geometry or instances have been modified.
    if (_instances.changedThisFrame() || _geometry.changedThisFrame())
    {
        _rayTracingPipeline.reset();
    }

    // If there is no ray tracing pipeliine (as it was never created or has been released) create
    // the pipeline and associated resources.
    if (!_rayTracingPipeline)
    {
        rebuildInstanceList();
        rebuildPipeline();
        // TODO: Should upload lights here too.
        rebuildAccelerationStructure();
        rebuildResourceBindings();

        return true;
    }

    return false;
}

void HGIScene::rebuildInstanceList()
{
    // At the start of update loop (called if update required) clear the current instance data list.
    _lstInstances.clear();
    _lstImages.clear();

    // Get default material.
    auto defaultMaterial = dynamic_pointer_cast<HGIMaterial>(_pDefaultMaterialResource->resource());

    // Add instance data for all the instances.
    for (HGIInstance& instance : _instances.active().resources<HGIInstance>())
    {
        // Get instance and material.
        auto pMtl = instance.material() ? instance.material() : defaultMaterial;

        // Create a hit group shader record for instance.
        InstanceShaderRecord record;
        record.geometry = instance.hgiGeometry()->bufferAddresses;
        record.material = pMtl->ubo()->GetDeviceAddress();

        // Update geometry flags.
        record.hasNormals   = record.geometry.normalBufferDeviceAddress != 0ull;
        record.hasTexCoords = record.geometry.texCoordBufferDeviceAddress != 0ull;

        // Get baseColor image from material.
        HGIImagePtr pBaseColorImage = nullptr;
        if (pMtl->textures().findTexture("base_color_image") != -1)
        {
            auto texture = pMtl->textures().getTexture("base_color_image");
            pBaseColorImage = dynamic_pointer_cast<HGIImage>(texture);
        }
        if (pBaseColorImage)
        {
            record.baseColorTextureIndex = (int)_lstImages.size();
            _lstImages.push_back(pBaseColorImage);
        }
        else
            record.baseColorTextureIndex = kInvalidTexureIndex;

        // Get specular roughness image from material.
        HGIImagePtr pSpecularRoughnessImage = nullptr;
        if (pMtl->textures().findTexture("specular_roughness_image") != -1)
        {
            pSpecularRoughnessImage = dynamic_pointer_cast<HGIImage>(pMtl->textures().getTexture("specular_roughness_image"));
        }
        if (pSpecularRoughnessImage)
        {
            record.specularRoughnessTextureIndex = (int)_lstImages.size();
            _lstImages.push_back(pSpecularRoughnessImage);
        }
        else
            record.specularRoughnessTextureIndex = kInvalidTexureIndex;

        // Get opacity image from material.
        HGIImagePtr pOpacityImage = nullptr;
        if (pMtl->textures().findTexture("opacity_image") != -1)
        {
            pOpacityImage = dynamic_pointer_cast<HGIImage>(pMtl->textures().getTexture("opacity_image"));
        }
        if (pOpacityImage)
        {
            record.opacityTextureIndex = (int)_lstImages.size();
            _lstImages.push_back(pOpacityImage);
        }
        else
            record.opacityTextureIndex = kInvalidTexureIndex;

        // Get normal image from material.
        HGIImagePtr pNormalImage = nullptr;
        if (pMtl->textures().findTexture("normal_image") != -1)
        {
            pNormalImage = dynamic_pointer_cast<HGIImage>(pMtl->textures().getTexture("normal_image"));
        }
        if (pNormalImage)
        {
            record.normalTextureIndex = (int)_lstImages.size();
            _lstImages.push_back(pNormalImage);
        }
        else
            record.normalTextureIndex = kInvalidTexureIndex;

        _lstInstances.push_back({ instance, record });
    }
}

void HGIScene::rebuildAccelerationStructure()
{
    // Create the TLAS decriptor.
    HgiAccelerationStructureInstanceGeometryDesc instDesc;

    // Add descriptor for each instance.
    for (size_t i = 0; i < _lstInstances.size(); i++)
    {
        InstanceData instData         = _lstInstances[i];
        HGIInstance& instance         = instData.instance;
        shared_ptr<HGIGeometry> pGeom = instance.hgiGeometry();

        // Add transform, group index and BLAS (from geometry).
        HgiAccelerationStructureInstanceDesc inst;
        inst.transform  = instance.transform();
        inst.groupIndex = static_cast<uint32_t>(i);
        inst.blas       = pGeom->accelStructure->handle();

        // Add to the descriptor.
        instDesc.instances.push_back(inst);
    }

    // Create the TLAS geometry (we place.all the instance in one geometry object.)
    _tlasGeom = HgiAccelerationStructureGeometryHandleWrapper::create(
        _pRenderer->hgi()->CreateAccelerationStructureGeometry(instDesc), _pRenderer->hgi());

    // Create TLAS descriptor containing only one peice of TLAS geometry.
    HgiAccelerationStructureDesc asDesc;
    asDesc.debugName = "Top Level AS";
    asDesc.geometry.push_back(_tlasGeom->handle());
    asDesc.type = HgiAccelerationStructureTypeTopLevel;
    _tlas       = HgiAccelerationStructureHandleWrapper::create(
        _pRenderer->hgi()->CreateAccelerationStructure(asDesc), _pRenderer->hgi());

    // Create commands to build acceleration structure.
    HgiAccelerationStructureCmdsUniquePtr accelStructCmds =
        _pRenderer->hgi()->CreateAccelerationStructureCmds();
    accelStructCmds->PushDebugGroup("Build TLAS cmds");
    accelStructCmds->Build({ _tlas->handle() }, { { (uint32_t)instDesc.instances.size() } });
    accelStructCmds->PopDebugGroup();

    // Run the commands to build acceleration structure.
    // TODO: Should not be blocking.
    _pRenderer->hgi()->SubmitCmds(accelStructCmds.get(), HgiSubmitWaitTypeWaitUntilCompleted);
}

// TODO: Implement ground plane.
void HGIScene::setGroundPlanePointer(const IGroundPlanePtr& /*pGroundPlane*/) {}

void HGIScene::rebuildResourceBindings()
{
    // Get the environment textures.
    HGIEnvironmentPtr pEnvironment =
        static_pointer_cast<HGIEnvironment>(_pEnvironmentResource->resource());
    HGIImagePtr pBackgroundImage =
        static_pointer_cast<HGIImage>(pEnvironment->values().asImage("background_image"));
    HGIImagePtr pLightImage =
        static_pointer_cast<HGIImage>(pEnvironment->values().asImage("light_image"));

    // Create resource bindings (binding indices must match
    // HgiRayTracingPipelineDescriptorSetLayoutDesc created in createPipeline.)
    HgiResourceBindingsDesc resourceBindingsDesc;
    resourceBindingsDesc.debugName = "Scene Resource Bindings";
    // One acceleration structure resource.
    resourceBindingsDesc.accelerationStructures.resize(1);
    resourceBindingsDesc.accelerationStructures[0].bindingIndex           = 0;
    resourceBindingsDesc.accelerationStructures[0].accelerationStructures = { tlas() };
    resourceBindingsDesc.accelerationStructures[0].resourceType =
        HgiBindResourceTypeAccelerationStructure;
    resourceBindingsDesc.accelerationStructures[0].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit;

    // 5 texture resources:
    //  - output image
    //  - array of textures for instances.)
    //  - Default sampler
    //  - Background image
    //  - Environment light image
    resourceBindingsDesc.textures.resize(6);

    // Add resource for output image storage texture resource.
    resourceBindingsDesc.textures[0].bindingIndex = 1;
    resourceBindingsDesc.textures[0].textures     = { _pRenderer->directTex() };
    resourceBindingsDesc.textures[0].samplers     = { _pRenderer->sampler() };
    resourceBindingsDesc.textures[0].resourceType = HgiBindResourceTypeStorageImage;
    resourceBindingsDesc.textures[0].stageUsage   = HgiShaderStageRayGen;

    // Add resources for array of textures and samplers for instances.
    resourceBindingsDesc.textures[1].bindingIndex = 3;
    if (_lstImages.empty())
    {
        // Add default texture+sampler to bindings, to avoid empty texture array.
        resourceBindingsDesc.textures[1].textures.push_back(_pDefaultImage->handle());
        resourceBindingsDesc.textures[1].samplers.push_back(_pRenderer->sampler());
    }
    else
    {
        // Add textures+samplers to pipeline, to avoid empty texture array.
        for (size_t i = 0; i < _lstImages.size(); i++)
        {
            resourceBindingsDesc.textures[1].textures.push_back(_lstImages[i]->texture());
            resourceBindingsDesc.textures[1].samplers.push_back(_pRenderer->sampler());
        }
    }
    resourceBindingsDesc.textures[1].resourceType = HgiBindResourceTypeCombinedSamplerImage;
    resourceBindingsDesc.textures[1].stageUsage   = HgiShaderStageClosestHit;

    // Add resource for default sampler resource.
    resourceBindingsDesc.textures[2].bindingIndex = 6;
    resourceBindingsDesc.textures[2].textures     = { _pDefaultImage->handle() };
    resourceBindingsDesc.textures[2].samplers     = { _pRenderer->sampler() };
    resourceBindingsDesc.textures[2].resourceType = HgiBindResourceTypeSampler;
    resourceBindingsDesc.textures[2].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    ;
    // Add resource for background image.
    resourceBindingsDesc.textures[3].bindingIndex = 7;
    resourceBindingsDesc.textures[3].textures     = { pBackgroundImage ? pBackgroundImage->texture()
                                                                       : _pDefaultImage->handle() };
    resourceBindingsDesc.textures[3].samplers     = { _pRenderer->sampler() };
    resourceBindingsDesc.textures[3].resourceType = HgiBindResourceTypeSampledImage;
    resourceBindingsDesc.textures[3].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    ;
    // Add resource for light image.
    resourceBindingsDesc.textures[4].bindingIndex = 8;
    resourceBindingsDesc.textures[4].textures     = { pLightImage ? pLightImage->texture()
                                                                  : _pDefaultImage->handle() };
    resourceBindingsDesc.textures[4].samplers     = { _pRenderer->sampler() };
    resourceBindingsDesc.textures[4].resourceType = HgiBindResourceTypeSampledImage;
    resourceBindingsDesc.textures[4].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    ;
    // accumulation texture
    resourceBindingsDesc.textures[5].bindingIndex = 9;
    resourceBindingsDesc.textures[5].textures     = { _pRenderer->accumulationTex() };
    resourceBindingsDesc.textures[5].samplers     = { _pRenderer->sampler() };
    resourceBindingsDesc.textures[5].resourceType = HgiBindResourceTypeStorageImage;
    resourceBindingsDesc.textures[5].stageUsage   = HgiShaderStageRayGen;


    // 3 buffer resources for:
    //  - frame data UBO
    //  - sample data UBO
    //  - environment data UBO
    resourceBindingsDesc.buffers.resize(3);
    resourceBindingsDesc.buffers[0].bindingIndex = 2;
    resourceBindingsDesc.buffers[0].buffers      = { _pRenderer->frameDataUbo() };
    resourceBindingsDesc.buffers[0].offsets      = { 0 };
    resourceBindingsDesc.buffers[0].resourceType = HgiBindResourceTypeUniformBuffer;
    resourceBindingsDesc.buffers[0].stageUsage   = HgiShaderStageRayGen | HgiShaderStageClosestHit;
    resourceBindingsDesc.buffers[1].bindingIndex = 4;
    resourceBindingsDesc.buffers[1].buffers      = { _pRenderer->sampleDataUbo() };
    resourceBindingsDesc.buffers[1].offsets      = { 0 };
    resourceBindingsDesc.buffers[1].resourceType = HgiBindResourceTypeUniformBuffer;
    resourceBindingsDesc.buffers[1].stageUsage   = HgiShaderStageRayGen | HgiShaderStageClosestHit;
    resourceBindingsDesc.buffers[2].bindingIndex = 5;
    resourceBindingsDesc.buffers[2].buffers      = { pEnvironment->ubo() };
    resourceBindingsDesc.buffers[2].offsets      = { 0 };
    resourceBindingsDesc.buffers[2].resourceType = HgiBindResourceTypeUniformBuffer;
    resourceBindingsDesc.buffers[2].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;

    // Create the resource bindings.
    auto& hgi    = _pRenderer->hgi();
    _resBindings = HgiResourceBindingsHandleWrapper::create(
        hgi->CreateResourceBindings(resourceBindingsDesc), hgi);
}

void HGIScene::createResources()
{
    // Get the HGI instance.
    auto& hgi = _pRenderer->hgi();

    // Create default 16x16 texture
    const int defaultTexDims[] = { 16, 16 };
    unsigned int defaultTexData[16 * 16 * 4];
    memset(defaultTexData, 0xff, sizeof(defaultTexData));
    HgiTextureDesc defaultTexDesc;
    defaultTexDesc.debugName      = "Default Texture";
    defaultTexDesc.format         = HgiFormat::HgiFormatUNorm8Vec4;
    defaultTexDesc.dimensions     = GfVec3i(16, 16, 1);
    defaultTexDesc.layerCount     = 1;
    defaultTexDesc.mipLevels      = 1;
    defaultTexDesc.initialData    = defaultTexData;
    defaultTexDesc.pixelsByteSize = defaultTexDims[0] * defaultTexDims[1] * sizeof(uint32_t);
    defaultTexDesc.usage          = HgiTextureUsageBitsShaderRead;
    _pDefaultImage = HgiTextureHandleWrapper::create(hgi->CreateTexture(defaultTexDesc), hgi);

    string transpilerErrors;
    // Common shader declarations required by all stages.
    string shaderDeclarations;
    if (hgi->GetAPIName() == HgiTokens->Metal)
    {
        shaderDeclarations = "#define MTL_TRANSLATE_GLSL\n";
    }

    // Transpile the main entry point shader.
    string mainEntryPointSource = CommonShaders::g_sMainEntryPoints;
    _transpiler->setSource("Options.slang", "");
    _transpiler->setSource("Definitions.slang", "");

    // Create the ray generation shader description including transpiled GLSL source.
    string rayGenShaderCode;
    // HgiRenderer can only have one main entry point for each shader source.
    // Disable the rest of the shader stages and use the ray generation shader.
    std::map<string, string> rayGenPreDefs = { { "DISABLE_HGI_SHADER_STAGE_MISS", "1" },
                                               { "DISABLE_HGI_SHADER_STAGE_CLOSEST_HIT", "1" },
                                               { "DISABLE_HGI_SHADER_STAGE_ANY_HIT", "1" } };
    if (!_transpiler->transpileCode(
            mainEntryPointSource, rayGenShaderCode, transpilerErrors, Transpiler::Language::GLSL, rayGenPreDefs))
    {
        AU_ERROR("Slang transpiling error on ray generation shader:\n%s", transpilerErrors.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Slang transpiling failed, see log in console for details.");
    }

    rayGenShaderCode += HGIShaders::g_sInstanceData;
    string rayGenShaderDeclarations = shaderDeclarations;
    HgiShaderFunctionDesc raygenShaderDesc;
    raygenShaderDesc.debugName              = "RayGenShader";
    raygenShaderDesc.shaderStage            = HgiShaderStageRayGen;
    raygenShaderDesc.shaderCode             = rayGenShaderCode.c_str();
    raygenShaderDesc.shaderCodeDeclarations = rayGenShaderDeclarations.c_str();
    // Compile the shader itself. fail and print errors if the compilation doesn't succeed.
    _rayGenShaderFunc =
        HgiShaderFunctionHandleWrapper::create(hgi->CreateShaderFunction(raygenShaderDesc), hgi);
    if (!_rayGenShaderFunc->handle()->IsValid())
    {
        std::string logString = _rayGenShaderFunc->handle()->GetCompileErrors();
        AU_ERROR("Error creating shader function for RayGenShader:\n%s", logString.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Shader function creation failed, see log in console for details.");
    }

    // Create the shadow miss shader description including GLSL source.
    string shadowMissShaderCode;
    // Disable the rest of the shader stages and use the shadow miss shader.
    std::map<string, string> shadowMissPreDefs = { { "DISABLE_HGI_SHADER_STAGE_RAY_GEN", "1" },
                                                   { "DISABLE_HGI_SHADER_STAGE_CLOSEST_HIT", "1" },
                                                   { "DISABLE_HGI_SHADER_STAGE_ANY_HIT", "1" } };
    if (!_transpiler->transpileCode(
            mainEntryPointSource, shadowMissShaderCode, transpilerErrors, Transpiler::Language::GLSL, shadowMissPreDefs))
    {
        AU_ERROR("Slang transpiling error on shadow miss shader:\n%s", transpilerErrors.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Slang transpiling failed, see log in console for details.");
    }

    string shadowMissShaderDeclarations = shaderDeclarations;
    HgiShaderFunctionDesc shadowMissShaderDesc;
    shadowMissShaderDesc.debugName              = "ShadowMissShader";
    shadowMissShaderDesc.shaderStage            = HgiShaderStageMiss;
    shadowMissShaderDesc.shaderCode             = shadowMissShaderCode.c_str();
    shadowMissShaderDesc.shaderCodeDeclarations = shadowMissShaderDeclarations.c_str();
    // Compile the shader itself. fail and print errors if the compilation doesn't succeed.
    _shadowMissShaderFunc = HgiShaderFunctionHandleWrapper::create(
        hgi->CreateShaderFunction(shadowMissShaderDesc), hgi);
    if (!_shadowMissShaderFunc->handle()->IsValid())
    {
        std::string logString = _shadowMissShaderFunc->handle()->GetCompileErrors();
        AU_ERROR(
            "Error creating shader function for ShadowMissShader:\n%s", logString.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Shader function creation failed, see log in console for details.");
    }

    // Create the closest hit shader description, appending the the raw instance data GLSL code.
    string closestHitShaderCode;
    // Disable the rest of the shader stages and use the closest hit shader.
    std::map<string, string> closestHitPreDefs = { { "DISABLE_HGI_SHADER_STAGE_RAY_GEN", "1" },
                                                  { "DISABLE_HGI_SHADER_STAGE_MISS", "1" },
                                                  { "DISABLE_HGI_SHADER_STAGE_ANY_HIT", "1" } };
    if (!_transpiler->transpileCode(
        mainEntryPointSource, closestHitShaderCode, transpilerErrors, Transpiler::Language::GLSL, closestHitPreDefs))
    {
        AU_ERROR("Slang transpiling error on closest hit shader:\n%s", transpilerErrors.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Slang transpiling failed, see log in console for details.");
    }

    closestHitShaderCode += HGIShaders::g_sInstanceData;
    string closestHitShaderDeclarations = shaderDeclarations;
    HgiShaderFunctionDesc closestHitShaderDesc;
    closestHitShaderDesc.debugName              = "ClosestHitShader";
    closestHitShaderDesc.shaderStage            = HgiShaderStageClosestHit;
    closestHitShaderDesc.shaderCode             = closestHitShaderCode.c_str();
    closestHitShaderDesc.shaderCodeDeclarations = closestHitShaderDeclarations.c_str();
    // Compile the shader itself. fail and print errors if the compilation doesn't succeed.
    _closestHitShaderFunc = HgiShaderFunctionHandleWrapper::create(
        hgi->CreateShaderFunction(closestHitShaderDesc), hgi);
    if (!_closestHitShaderFunc->handle()->IsValid())
    {
        std::string logString = _closestHitShaderFunc->handle()->GetCompileErrors();
        AU_ERROR("Error creating shader function for closest hit shader:\n%s", logString.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Shader function creation failed, see log in console for details.");
    }

    /* TODO: Add any hit shader.
    // Create the any hit shader description, appending the the raw instance data GLSL code.
    string anyHitShaderCode;
    // Disable the rest of the shader stages and use the any hit shader.
    std::map<string, string> anyHitPreDefs = { { "DISABLE_HGI_SHADER_STAGE_RAY_GEN", "1" },
                                              { "DISABLE_HGI_SHADER_STAGE_MISS", "1" },
                                              { "DISABLE_HGI_SHADER_STAGE_CLOSEST_HIT", "1" } };
    if (!_transpiler->transpileCode(
        mainEntryPointSource, anyHitShaderCode, transpilerErrors, Transpiler::Language::GLSL, anyHitPreDefs))
    {
        AU_ERROR("Slang transpiling error on any hit shader:\n%s", transpilerErrors.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Slang transpiling failed, see log in console for details.");
    }

    anyHitShaderCode += HGIShaders::g_sInstanceData;
    string anyHitShaderDeclarations = shaderDeclarations;
    HgiShaderFunctionDesc anyHitShaderDesc;
    anyHitShaderDesc.debugName   = "AnyHitShader";
    anyHitShaderDesc.shaderStage = HgiShaderStageAnyHit;
    anyHitShaderDesc.shaderCode  = anyHitShaderCode.c_str();
    anyHitShaderDesc.shaderCodeDeclarations = anyHitShaderDeclarations.c_str();
    // Compile the shader itself. fail and print errors if the compilation doesn't succeed.
    _anyHitShaderFunc = HgiShaderFunctionHandleWrapper::create(
        hgi->CreateShaderFunction(anyHitShaderDesc), hgi);
    if (!_anyHitShaderFunc->handle()->IsValid())
    {
        std::string logString = _anyHitShaderFunc->handle()->GetCompileErrors();
        AU_ERROR("Error creating shader function for any hit shader:\n%s", logString.c_str());
        AU_DEBUG_BREAK();
        AU_FAIL("Shader function creation failed, see log in console for details.");
    }
    */
}

void HGIScene::rebuildPipeline()
{
    // Get the HGI instance.
    auto& hgi = _pRenderer->hgi();

    // Create the global resources if required.
    if (!_pDefaultImage)
    {
        createResources();
    }

    // Create the descriptor set layour description for all the global resources.
    // 9 global resources:
    //   - acceleration structure
    //   - output direct light image
    //   - frame data UBO
    //   - texture + sampler array
    //   - sample data UBO
    //   - default sampler
    //   - background image
    //   - light image
    HgiRayTracingPipelineDescriptorSetLayoutDesc layoutBinding;
    layoutBinding.resourceBinding.resize(10);
    // Description of acceleration structure.
    layoutBinding.resourceBinding[0].bindingIndex = 0;
    layoutBinding.resourceBinding[0].count        = 1;
    layoutBinding.resourceBinding[0].resourceType = HgiBindResourceTypeAccelerationStructure;
    layoutBinding.resourceBinding[0].stageUsage   = HgiShaderStageRayGen | HgiShaderStageClosestHit;
    // Description of output direct light image.
    layoutBinding.resourceBinding[1].bindingIndex = 1;
    layoutBinding.resourceBinding[1].count        = 1;
    layoutBinding.resourceBinding[1].resourceType = HgiBindResourceTypeStorageImage;
    layoutBinding.resourceBinding[1].stageUsage   = HgiShaderStageRayGen;
    // Description of frame data UBO
    layoutBinding.resourceBinding[2].bindingIndex = 2;
    layoutBinding.resourceBinding[2].count        = 1;
    layoutBinding.resourceBinding[2].resourceType = HgiBindResourceTypeUniformBuffer;
    layoutBinding.resourceBinding[2].stageUsage   = HgiShaderStageRayGen | HgiShaderStageClosestHit;
    // Description of array of textures and samplers (shared by all instances)
    // Minimum size one.
    layoutBinding.resourceBinding[3].bindingIndex = 3;
    layoutBinding.resourceBinding[3].count = _lstImages.empty() ? 1 : (uint32_t)_lstImages.size();
    layoutBinding.resourceBinding[3].resourceType = HgiBindResourceTypeCombinedSamplerImage;
    layoutBinding.resourceBinding[3].stageUsage   = HgiShaderStageClosestHit;
    // Description of sample data UBO
    layoutBinding.resourceBinding[4].bindingIndex = 4;
    layoutBinding.resourceBinding[4].count        = 1;
    layoutBinding.resourceBinding[4].resourceType = HgiBindResourceTypeUniformBuffer;
    layoutBinding.resourceBinding[4].stageUsage   = HgiShaderStageRayGen | HgiShaderStageClosestHit;
    // Description of environment UBO
    layoutBinding.resourceBinding[5].bindingIndex = 5;
    layoutBinding.resourceBinding[5].count        = 1;
    layoutBinding.resourceBinding[5].resourceType = HgiBindResourceTypeUniformBuffer;
    layoutBinding.resourceBinding[5].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    // Description of default sampler
    layoutBinding.resourceBinding[6].bindingIndex = 6;
    layoutBinding.resourceBinding[6].count        = 1;
    layoutBinding.resourceBinding[6].resourceType = HgiBindResourceTypeSampler;
    layoutBinding.resourceBinding[6].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    // Description of background image
    layoutBinding.resourceBinding[7].bindingIndex = 7;
    layoutBinding.resourceBinding[7].count        = 1;
    layoutBinding.resourceBinding[7].resourceType = HgiBindResourceTypeSampledImage;
    layoutBinding.resourceBinding[7].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    // Description of light image
    layoutBinding.resourceBinding[8].bindingIndex = 8;
    layoutBinding.resourceBinding[8].count        = 1;
    layoutBinding.resourceBinding[8].resourceType = HgiBindResourceTypeSampledImage;
    layoutBinding.resourceBinding[8].stageUsage =
        HgiShaderStageRayGen | HgiShaderStageClosestHit | HgiShaderStageMiss;
    // Description of accumulation texture.
    layoutBinding.resourceBinding[9].bindingIndex = 9;
    layoutBinding.resourceBinding[9].count        = 1;
    layoutBinding.resourceBinding[9].resourceType = HgiBindResourceTypeStorageImage;
    layoutBinding.resourceBinding[9].stageUsage   = HgiShaderStageRayGen;

    // Create pipeline description.
    HgiRayTracingPipelineDesc pipelineDesc;
    pipelineDesc.debugName            = "Main Raytracing Pipeline";
    pipelineDesc.maxRayRecursionDepth = 10;
    pipelineDesc.descriptorSetLayouts.push_back(layoutBinding);
    // 4 shaders (ray gen, shadow miss, closest hit and any hit)
    pipelineDesc.shaders.resize(3);
    //pipelineDesc.shaders.resize(4);
    pipelineDesc.shaders[0].shader     = _rayGenShaderFunc->handle();
    pipelineDesc.shaders[0].entryPoint = "main";
    pipelineDesc.shaders[1].shader     = _shadowMissShaderFunc->handle();
    pipelineDesc.shaders[1].entryPoint = "main";
    pipelineDesc.shaders[2].shader     = _closestHitShaderFunc->handle();
    pipelineDesc.shaders[2].entryPoint = "main";
    //pipelineDesc.shaders[3].shader     = _anyHitShaderFunc->handle();
    //pipelineDesc.shaders[3].entryPoint = "main";

    // Three general shader groups (for miss and ray gen) and one triangle shader group for each
    // instance.
    pipelineDesc.groups.resize(2 + _lstInstances.size());
    //pipelineDesc.groups.resize(2 + (_lstInstances.size() * 2));
    // Ray gen shader group.
    pipelineDesc.groups[0].type          = HgiRayTracingShaderGroupTypeGeneral;
    pipelineDesc.groups[0].generalShader = 0; // Index within shader array above.
    // Shadow miss shader group.
    pipelineDesc.groups[1].type          = HgiRayTracingShaderGroupTypeGeneral;
    pipelineDesc.groups[1].generalShader = 1; // Index within shader array above.
    // Triangle shader groups for each instance.
    for (size_t i = 0; i < _lstInstances.size(); i++)
    {
        size_t shaderRecordStride = sizeof(_lstInstances[i].shaderRecord);
        // Triangle miss shader group with closest hit shader.
        pipelineDesc.groups[2 + i].type             = HgiRayTracingShaderGroupTypeTriangles;
        pipelineDesc.groups[2 + i].closestHitShader = 2; // Index within shader array above.
        // Add the hit group record structure to the shader record (this will copied after the
        // shader handle in the shader binding table.)
        pipelineDesc.groups[2 + i].pShaderRecord      = &_lstInstances[i].shaderRecord;
        pipelineDesc.groups[2 + i].shaderRecordLength = shaderRecordStride;

        /*
        // Triangle miss shader group with any hit shader.
        i++;
        pipelineDesc.groups[2 + i].type         = HgiRayTracingShaderGroupTypeTriangles;
        pipelineDesc.groups[2 + i].anyHitShader = 3; // Index within shader array above.
        // Add the hit group record structure to the shader record (this will copied after the
        // shader handle in the shader binding table.)
        pipelineDesc.groups[2 + i].pShaderRecord      = &_lstInstances[i].shaderRecord;
        pipelineDesc.groups[2 + i].shaderRecordLength = shaderRecordStride;
        */
    }

    // Create the pipeline.
    _rayTracingPipeline = HgiRayTracingPipelineHandleWrapper::create(
        hgi->CreateRayTracingPipeline(pipelineDesc), hgi);
}

ILightPtr HGIScene::addLightPointer(const string& lightType)
{
    // Only distant lights are currently supported.
    AU_ASSERT(lightType.compare(Names::LightTypes::kDistantLight) == 0,
        "Only distant lights currently supported");

    // Assign arbritary index to ensure deterministic ordering.
    int index = _currentLightIndex++;

    // Create the light object.
    HGILightPtr pLight = make_shared<HGILight>(this, lightType, index);

    // Add weak pointer to distant light map.
    _distantLights[index] = pLight;

    // Return the new light.
    return pLight;
}

IInstancePtr HGIScene::addInstancePointer(const Path& /* path*/, const IGeometryPtr& pGeom,
    const IMaterialPtr& pMaterial, const mat4& transform,
    const LayerDefinitions& /* materialLayers*/)
{
    // Cast the (optional) material to the device implementation. Use the default material if one is
    // not specified.
    HGIMaterialPtr pHGIMaterial = pMaterial
        ? dynamic_pointer_cast<HGIMaterial>(pMaterial)
        : dynamic_pointer_cast<HGIMaterial>(_pDefaultMaterialResource->resource());

    // Create the instance object and add it to the list of instances for the scene.
    HGIInstancePtr pHGIInstance = make_shared<HGIInstance>(
        _pRenderer, pHGIMaterial, dynamic_pointer_cast<HGIGeometry>(pGeom), transform);

    return pHGIInstance;
}

END_AURORA
