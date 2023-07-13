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

#include "CompiledShaders/CommonShaders.h"
#include "CompiledShaders/HGIShaders.h"
#include "HGIEnvironment.h"
#include "HGIGroundPlane.h"
#include "HGIImage.h"
#include "HGIMaterial.h"
#include "HGIRenderBuffer.h"
#include "HGIRenderer.h"
#include "HGIScene.h"
#include "HGIWindow.h"

using namespace pxr;

BEGIN_AURORA

const vector<string> DefaultEntryPoints = { "RADIANCE_HIT" };

HGIRenderer::HGIRenderer(uint32_t activeFrameCount) :
    RendererBase(activeFrameCount), _shaderLibrary(DefaultEntryPoints)
{

    _isValid = true;

    // Create the HGI instance.
    _hgi = pxr::Hgi::CreatePlatformDefaultHgi(HgiDeviceCapabilitiesBitsRayTracing);
    AU_ASSERT(hgi(), "Failed to create Hgi instance.");

    // Vulkan texture coordinates are upside down compared to DX.
    _pAssetMgr->enableVerticalFlipOnImageLoad(!_values.asBoolean(kLabelIsFlipImageYEnabled));

    MaterialShaderSource defaultMaterialSource(
        "Default", CommonShaders::g_sInitializeDefaultMaterialType);

    // Create the material definition for default shader.
    _pDefaultMaterialDefinition = make_shared<MaterialDefinition>(defaultMaterialSource,
        MaterialBase::StandardSurfaceDefaults, MaterialBase::updateBuiltInMaterial, false);

    // Create shader from the definition.
    MaterialShaderDefinition shaderDef;
    _pDefaultMaterialDefinition->getShaderDefinition(shaderDef);
    _pDefaultMaterialShader = _shaderLibrary.acquire(shaderDef);

    // Ensure the radiance hit entry point is compiled for default shader.
    _pDefaultMaterialShader->incrementRefCount("RADIANCE_HIT");
}
HGIRenderer::~HGIRenderer()
{
    // Ensure scene deleted before resources are destroyed.
    _pScene.reset();

    // Destroy all the GPU resources.
    _frameDataUbo.reset();
    _sampleDataUbo.reset();
    _postProcessingUbo.reset();
    _accumulationComputeResourceBindings.reset();
    _accumulationComputePipeline.reset();
    _postProcessComputeResourceBindings.reset();
    _postProcessComputePipeline.reset();
    _accumulationComputeResourceBindings.reset();
    _accumComputeShader.reset();
    _accumComputeShaderProgram.reset();
    _postProcessComputeShader.reset();
    _postProcessComputeShaderProgram.reset();
    _pDirectTex.reset();
    _pAccumulationTex.reset();
    _sampler.reset();

    // Destroy the HGI device.
    _hgi.reset();
}

void HGIRenderer::createResources()
{
    HGIScenePtr pHGIScene = hgiScene();

    // Create a default sampler, used by all textures.
    HgiSamplerDesc samplerDesc;
    _sampler = HgiSamplerHandleWrapper::create(hgi()->CreateSampler(samplerDesc), hgi());

    // Create UBO buffer description for frame data.
    // TODO: Allow modification of frame data.
    HgiBufferDesc rtUboDesc;
    rtUboDesc.debugName = "Raytracing frame data UBO";
    rtUboDesc.usage     = HgiBufferUsageUniform;
    rtUboDesc.byteSize  = sizeof(FrameData);

    // Create UBO buffer object.
    _frameDataUbo = HgiBufferHandleWrapper::create(hgi()->CreateBuffer(rtUboDesc), hgi());

    // Create UBO buffer description for sample data.
    HgiBufferDesc sampleDataUboDesc;
    sampleDataUboDesc.debugName = "Raytracing sample data UBO";
    sampleDataUboDesc.usage     = HgiBufferUsageUniform;
    sampleDataUboDesc.byteSize  = sizeof(SampleData);

    // Create UBO buffer object.
    _sampleDataUbo = HgiBufferHandleWrapper::create(hgi()->CreateBuffer(sampleDataUboDesc), hgi());

    // Create UBO buffer description for frame data.
    // TODO: Allow modification of post-processing data.
    HgiBufferDesc postProcUboDesc;
    postProcUboDesc.debugName = "Post processing data UBO";
    postProcUboDesc.usage     = HgiBufferUsageUniform;
    postProcUboDesc.byteSize  = sizeof(PostProcessing);

    // Create post processing UBO buffer object.
    _postProcessingUbo =
        HgiBufferHandleWrapper::create(hgi()->CreateBuffer(postProcUboDesc), hgi());

    // All render buffers are RGBA32f
    HgiFormat hgiFormat = HgiFormat::HgiFormatFloat32Vec4;

    // Create direct lighting buffer storage texture.
    HgiTextureDesc directTexDesc;
    directTexDesc.debugName  = "Direct Light Result Texture";
    directTexDesc.format     = hgiFormat;
    directTexDesc.dimensions = GfVec3i(_pRenderBuffer->width(), _pRenderBuffer->height(), 1);
    directTexDesc.layerCount = 1;
    directTexDesc.mipLevels  = 1;
    directTexDesc.usage      = HgiTextureUsageBitsShaderRead | HgiTextureUsageBitsShaderWrite;
    _pDirectTex = HgiTextureHandleWrapper::create(hgi()->CreateTexture(directTexDesc), hgi());

    // Create accumulation storage texture.
    HgiTextureDesc accumTexDesc;
    accumTexDesc.debugName  = "Accumulation Result Texture";
    accumTexDesc.format     = hgiFormat;
    accumTexDesc.dimensions = GfVec3i(_pRenderBuffer->width(), _pRenderBuffer->height(), 1);
    accumTexDesc.layerCount = 1;
    accumTexDesc.mipLevels  = 1;
    accumTexDesc.usage      = HgiTextureUsageBitsShaderRead | HgiTextureUsageBitsShaderWrite;
    _pAccumulationTex = HgiTextureHandleWrapper::create(hgi()->CreateTexture(accumTexDesc), hgi());

    // Description of shader function for post-processing compute shader.
    HgiShaderFunctionDesc postProcessComputeDesc;
    postProcessComputeDesc.shaderCode                  = HGIShaders::g_sPostProcessing.c_str();
    postProcessComputeDesc.debugName                   = "postProcessulationComputeShader";
    postProcessComputeDesc.shaderStage                 = HgiShaderStageCompute;
    postProcessComputeDesc.computeDescriptor.localSize = GfVec3i(8, 8, 1);

    // Use the HGI code generation to add shader inputs to post-processing shader function.
    HgiShaderFunctionAddWritableTexture(
        &postProcessComputeDesc, "outTexture", 2, HgiFormatFloat32Vec4);
    HgiShaderFunctionAddTexture(&postProcessComputeDesc, "accumulationTexture");
    HgiShaderFunctionAddStageInput(&postProcessComputeDesc, "hd_GlobalInvocationID", "uvec3",
        HgiShaderKeywordTokens->hdGlobalInvocationID);

    // Create post-processing shader function, abort application if compilation fails.
    _postProcessComputeShader = HgiShaderFunctionHandleWrapper::create(
        hgi()->CreateShaderFunction(postProcessComputeDesc), hgi());
    if (!_postProcessComputeShader->handle()->IsValid())
    {
        std::string logString = _postProcessComputeShader->handle()->GetCompileErrors();
        AU_INFO("%s", postProcessComputeDesc.shaderCode);
        AU_FAIL("Compile error in post process compute shader:\n%s", logString.c_str());
    }

    // Create post-processing shader program, abort application if linking fails.
    HgiShaderProgramDesc postProcessComputeProgramDesc;
    postProcessComputeProgramDesc.shaderFunctions = { _postProcessComputeShader->handle() };
    postProcessComputeProgramDesc.debugName       = "PostProcessingComputeShaderProgram";
    _postProcessComputeShaderProgram              = HgiShaderProgramHandleWrapper::create(
        hgi()->CreateShaderProgram(postProcessComputeProgramDesc), hgi());
    AU_ASSERT(_postProcessComputeShaderProgram->handle()->IsValid(), "Invalid shader program");

    // Description of shader function for accumulation compute shader.
    HgiShaderFunctionDesc accumComputeDesc;
    accumComputeDesc.shaderCode                  = HGIShaders::g_sAccumulation.c_str();
    accumComputeDesc.debugName                   = "AccumulationComputeShader";
    accumComputeDesc.shaderStage                 = HgiShaderStageCompute;
    accumComputeDesc.computeDescriptor.localSize = GfVec3i(8, 8, 1);

    // Use the HGI code generation to add shader inputs to accumulation shader function.
    HgiShaderFunctionAddWritableTexture(&accumComputeDesc, "outTexture", 2, HgiFormatFloat32Vec4);
    HgiShaderFunctionAddTexture(&accumComputeDesc, "accumulationTexture");
    HgiShaderFunctionAddTexture(&accumComputeDesc, "directLightTexture");
    HgiShaderFunctionAddStageInput(&accumComputeDesc, "hd_GlobalInvocationID", "uvec3",
        HgiShaderKeywordTokens->hdGlobalInvocationID);

    // Create accumulation shader function, abort application if compilation fails.
    _accumComputeShader = HgiShaderFunctionHandleWrapper::create(
        hgi()->CreateShaderFunction(accumComputeDesc), hgi());
    if (!_accumComputeShader->handle()->IsValid())
    {
        std::string logString = _accumComputeShader->handle()->GetCompileErrors();
        AU_INFO("%s", accumComputeDesc.shaderCode);
        AU_FAIL("Compile error in accumulation compute shader:\n%s", logString.c_str());
    }

    // Create accumulation shader program, abort applicatoin if linking fails.
    HgiShaderProgramDesc accumComputeProgramDesc;
    accumComputeProgramDesc.shaderFunctions = { _accumComputeShader->handle() };
    accumComputeProgramDesc.debugName       = "AccumulationComputeShaderProgram";
    _accumComputeShaderProgram              = HgiShaderProgramHandleWrapper::create(
        hgi()->CreateShaderProgram(accumComputeProgramDesc), hgi());
    AU_ASSERT(_accumComputeShaderProgram->handle()->IsValid(), "Invalid shader program");

    // Create binding description for accumulation texture as input texture.
    HgiTextureBindDesc accumInTexBind;
    accumInTexBind.bindingIndex = 1;
    accumInTexBind.stageUsage   = HgiShaderStageCompute;
    accumInTexBind.textures.push_back(_pAccumulationTex->handle());
    accumInTexBind.samplers.push_back(sampler());
    accumInTexBind.resourceType = HgiBindResourceTypeCombinedSamplerImage;

    // Create binding description for accumulation texture as output storage texture.
    HgiTextureBindDesc accumTexBind;
    accumTexBind.bindingIndex = 0;
    accumTexBind.stageUsage   = HgiShaderStageCompute;
    accumTexBind.textures.push_back(_pAccumulationTex->handle());
    accumTexBind.resourceType = HgiBindResourceTypeStorageImage;

    // Create binding description for final render target as storage texture.
    HgiTextureBindDesc finalTexBind;
    finalTexBind.bindingIndex = 0;
    finalTexBind.stageUsage   = HgiShaderStageCompute;
    finalTexBind.textures     = { renderBuffer()->storageTex() };
    finalTexBind.resourceType = HgiBindResourceTypeStorageImage;

    // Create binding description for direct light texture as input texture.
    HgiTextureBindDesc directTexBind;
    directTexBind.bindingIndex = 2;
    directTexBind.stageUsage   = HgiShaderStageCompute;
    directTexBind.textures.push_back(_pDirectTex->handle());
    directTexBind.samplers.push_back(sampler());
    directTexBind.resourceType = HgiBindResourceTypeCombinedSamplerImage;

    // Create binding description for sample data UBO.
    HgiBufferBindDesc bufferDesc0;
    bufferDesc0.bindingIndex = 3;
    bufferDesc0.buffers      = { _sampleDataUbo->handle() };
    bufferDesc0.offsets      = { 0 };
    bufferDesc0.resourceType = HgiBindResourceTypeUniformBuffer;
    bufferDesc0.stageUsage   = HgiShaderStageCompute;

    // Create binding description for post processing UBO.
    HgiBufferBindDesc bufferDesc1;
    bufferDesc1.bindingIndex = 3;
    bufferDesc1.buffers      = { _postProcessingUbo->handle() };
    bufferDesc1.offsets      = { 0 };
    bufferDesc1.resourceType = HgiBindResourceTypeUniformBuffer;
    bufferDesc1.stageUsage   = HgiShaderStageCompute;

    // Create resource description for accumulation compute shader.
    HgiResourceBindingsDesc resourceDesc;
    resourceDesc.debugName = "AccumulationCompueShaderResources";
    resourceDesc.buffers.push_back(std::move(bufferDesc0));
    resourceDesc.textures.push_back(accumTexBind);
    resourceDesc.textures.push_back(accumInTexBind);
    resourceDesc.textures.push_back(directTexBind);
    _accumulationComputeResourceBindings = HgiResourceBindingsHandleWrapper::create(
        hgi()->CreateResourceBindings(resourceDesc), hgi());

    // Create resource descriptoin for post-processing compute shader.
    HgiResourceBindingsDesc postProcessResourceDesc;
    postProcessResourceDesc.debugName = "PostProcessComputeShaderResources";
    postProcessResourceDesc.buffers.push_back(std::move(bufferDesc1));
    postProcessResourceDesc.textures.push_back(finalTexBind);
    postProcessResourceDesc.textures.push_back(accumInTexBind);
    _postProcessComputeResourceBindings = HgiResourceBindingsHandleWrapper::create(
        hgi()->CreateResourceBindings(postProcessResourceDesc), hgi());

    // Create compute pipeline for accumulation shader.
    HgiComputePipelineDesc pipelineDesc;
    pipelineDesc.debugName     = "AccumulationComputePipeline";
    pipelineDesc.shaderProgram = _accumComputeShaderProgram->handle();
    _accumulationComputePipeline =
        HgiComputePipelineHandleWrapper::create(hgi()->CreateComputePipeline(pipelineDesc), hgi());

    // Create compute pipeline for post-processing shader.
    HgiComputePipelineDesc postProcessPipelineDesc;
    postProcessPipelineDesc.debugName     = "PostProcessComputePipeline";
    postProcessPipelineDesc.shaderProgram = _postProcessComputeShaderProgram->handle();
    _postProcessComputePipeline           = HgiComputePipelineHandleWrapper::create(
        hgi()->CreateComputePipeline(postProcessPipelineDesc), hgi());
}

IWindowPtr HGIRenderer::createWindow(WindowHandle window, uint32_t width, uint32_t height)
{
    // Create dummy window object.
    // TODO: Support windows (or just remove windows from API)
    return std::make_shared<HGIWindow>(this, window, width, height);
}

IRenderBufferPtr HGIRenderer::createRenderBuffer(int width, int height, ImageFormat imageFormat)
{
    // create a render buffer.
    return std::make_shared<HGIRenderBuffer>(this, width, height, imageFormat);
}

ISamplerPtr HGIRenderer::createSamplerPointer(const Properties& /* props*/)
{
    // Return null.
    // TODO: Support sampler.
    return nullptr;
}

IImagePtr HGIRenderer::createImagePointer(const IImage::InitData& initData)
{
    // Create a return a new image object.
    return make_shared<HGIImage>(this, initData);
}

IMaterialPtr HGIRenderer::createMaterialPointer(
    const std::string& materialType, const std::string& /*document*/, const std::string& name)
{
    if (materialType.compare(Names::MaterialTypes::kBuiltIn) != 0)
    {
        // Print error and return null material type if material type not found.
        // TODO: Proper error handling for this case.
        AU_ERROR(
            "Unrecognized material type %s for material %s", materialType.c_str(), name.c_str());
        return nullptr;
    }

    // Create and return a new material object.
    return make_shared<HGIMaterial>(this, _pDefaultMaterialShader, _pDefaultMaterialDefinition);
}

IGeometryPtr HGIRenderer::createGeometryPointer(
    const GeometryDescriptor& desc, const std::string& name)
{
    // Create and return a new geometry object.
    return make_shared<HGIGeometry>(this, name, desc);
}

IEnvironmentPtr HGIRenderer::createEnvironmentPointer()
{
    // Create and return a new environment object.
    return make_shared<HGIEnvironment>(this);
}

IGroundPlanePtr HGIRenderer::createGroundPlanePointer()
{
    // Create dummy ground plane object.
    // TODO: Support ground plane.
    return make_shared<HGIGroundPlane>(this);
}

void HGIRenderer::render(uint32_t sampleStart, uint32_t sampleCount)
{
    HGIScenePtr pHGIScene = hgiScene();

    // Create renderer resources if needed (only done once, even if scene changes).
    if (!_frameDataUbo)
    {
        createResources();
    }

    // Update the scene (will update any GPU resources that have changed)
    pHGIScene->update();

    // Must have opaque shadows on HGI currently.
    _values.setValue(kLabelIsOpaqueShadowsEnabled, true);

    // Render all the samples.
    for (uint32_t i = 0; i < sampleCount; i++)
    {
        uint32_t sampleIndex = sampleStart + i;

        // Each sample is a separate HGI frame.
        hgi()->StartFrame();

        // Update the frame data UBO using staging buffer, if needed.
        if (updateFrameDataGPUStruct(getStagingAddress<FrameData>(_frameDataUbo)))
        {
            pxr::HgiBlitCmdsUniquePtr blitCmds = hgi()->CreateBlitCmds();
            pxr::HgiBufferCpuToGpuOp blitOp;
            blitOp.byteSize              = sizeof(FrameData);
            blitOp.cpuSourceBuffer       = getStagingAddress<FrameData>(_frameDataUbo);
            blitOp.sourceByteOffset      = 0;
            blitOp.gpuDestinationBuffer  = _frameDataUbo->handle();
            blitOp.destinationByteOffset = 0;
            blitCmds->CopyBufferCpuToGpu(blitOp);
            hgi()->SubmitCmds(blitCmds.get());
        }

        // Update the post processing data UBO using staging buffer, if needed.
        if (updatePostProcessingGPUStruct(getStagingAddress<PostProcessing>(_postProcessingUbo)))
        {
            pxr::HgiBlitCmdsUniquePtr blitCmds = hgi()->CreateBlitCmds();
            pxr::HgiBufferCpuToGpuOp blitOp;
            blitOp.byteSize = sizeof(PostProcessing);
            blitOp.cpuSourceBuffer =
                getStagingAddress<PostProcessing>(_postProcessingUbo->handle());
            blitOp.sourceByteOffset      = 0;
            blitOp.gpuDestinationBuffer  = _postProcessingUbo->handle();
            blitOp.destinationByteOffset = 0;
            blitCmds->CopyBufferCpuToGpu(blitOp);
            hgi()->SubmitCmds(blitCmds.get());
        }

        // Update the sample data with the current sample index.
        SampleData* pSampleDataStaging  = getStagingAddress<SampleData>(_sampleDataUbo);
        pSampleDataStaging->sampleIndex = sampleIndex;
        pSampleDataStaging->seedOffset  = 0;

        // Update the sample data on the GPU for every iteration of loop.
        pxr::HgiBlitCmdsUniquePtr blitCmds = hgi()->CreateBlitCmds();
        pxr::HgiBufferCpuToGpuOp blitOp;
        blitOp.byteSize              = sizeof(SampleData);
        blitOp.cpuSourceBuffer       = pSampleDataStaging;
        blitOp.sourceByteOffset      = 0;
        blitOp.gpuDestinationBuffer  = _sampleDataUbo->handle();
        blitOp.destinationByteOffset = 0;
        blitCmds->CopyBufferCpuToGpu(blitOp);
        hgi()->SubmitCmds(blitCmds.get());

        // Create ray tracing commands for frame.
        HgiRayTracingCmdsUniquePtr rtCmds = hgi()->CreateRayTracingCmds();
        rtCmds->PushDebugGroup("Frame Render RT commands");
        rtCmds->BindPipeline(pHGIScene->rayTracingPipeline());
        rtCmds->BindResources(pHGIScene->resourceBindings());
        rtCmds->TraceRays(_pRenderBuffer->width(), _pRenderBuffer->height(), 1);
        rtCmds->PopDebugGroup();

        // Submit the ray tracing commands for frame.
        hgi()->SubmitCmds(rtCmds.get());

        // Create accumulation compute commands for frame.
        HgiComputeCmdsUniquePtr computeCmds = hgi()->CreateComputeCmds();
        computeCmds->PushDebugGroup("Accumulation Compute Commands");
        computeCmds->BindResources(_accumulationComputeResourceBindings->handle());
        computeCmds->BindPipeline(_accumulationComputePipeline->handle());
        computeCmds->Dispatch(_pRenderBuffer->width(), _pRenderBuffer->height());

        // Submit the accumulation commands (which will update the accumulation buffer from the
        // direct light buffer created by ray tracing)
        hgi()->SubmitCmds(computeCmds.get());

        // If this is the last frame, run the post processing compute shader, to get final image.
        if (i == sampleCount - 1)
        {
            HgiComputeCmdsUniquePtr postProcessComputeCmds = hgi()->CreateComputeCmds();
            postProcessComputeCmds->PushDebugGroup("Post Process Compute Commands");
            postProcessComputeCmds->BindResources(_postProcessComputeResourceBindings->handle());
            postProcessComputeCmds->BindPipeline(_postProcessComputePipeline->handle());
            postProcessComputeCmds->Dispatch(_pRenderBuffer->width(), _pRenderBuffer->height());
            hgi()->SubmitCmds(postProcessComputeCmds.get());
        }

        // End HGI frame rendering.
        hgi()->EndFrame();
    }
}

void HGIRenderer::waitForTask()
{
    // TODO: Non-blocking commands.
}

IScenePtr HGIRenderer::createScene()
{
    // Return new scene object.
    return make_shared<HGIScene>(this);
}

void HGIRenderer::setScene(const IScenePtr& pScene)
{
    // Assign the new scene.
    _pScene = dynamic_pointer_cast<SceneBase>(pScene);
}

void HGIRenderer::setTargets(const TargetAssignments& targetAssignments)
{
    // Only use kFinal render target currently.
    _pRenderBuffer = (HGIRenderBuffer*)targetAssignments.at(AOV::kFinal).get();
}

const std::vector<std::string>& HGIRenderer::builtInMaterials()
{
    // TODO: No builtins currently.
    static const std::vector<std::string> sBuiltins;
    return sBuiltins;
}

END_AURORA
