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
#pragma once

#include "RendererBase.h"

#include "HGIHandleWrapper.h"
#include "HGIMaterial.h"
#include "HGIScene.h"

BEGIN_AURORA

// Conveinence function to get cast pointer from buffer's staging buffers
template <typename BufferType>
BufferType* getStagingAddress(pxr::HgiBufferHandle pBuffer)
{
    AU_ASSERT(pBuffer->GetByteSizeOfResource() >= sizeof(BufferType), "Buffer too small");
    return static_cast<BufferType*>(pBuffer->GetCPUStagingAddress());
}

template <typename BufferType>
BufferType* getStagingAddress(HgiBufferHandleWrapper::Pointer& pBuffer)
{
    return getStagingAddress<BufferType>(pBuffer->handle());
}

// An rasterization (HGI) implementation for IRenderer.
class HGIRenderer : public RendererBase
{
public:
    /*** Lifetime Management ***/

    HGIRenderer(uint32_t activeFrameCount);
    ~HGIRenderer();

    /*** IRenderer Functions ***/

    IWindowPtr createWindow(WindowHandle handle, uint32_t width, uint32_t height) override;
    IRenderBufferPtr createRenderBuffer(int width, int height, ImageFormat imageFormat) override;
    IImagePtr createImagePointer(const IImage::InitData& initData) override;
    ISamplerPtr createSamplerPointer(const Properties& props) override;
    IMaterialPtr createMaterialPointer(const string& materialType = Names::MaterialTypes::kBuiltIn,
        const string& document = "Default", const string& name = "") override;
    IScenePtr createScene() override;
    IEnvironmentPtr createEnvironmentPointer() override;
    IGeometryPtr createGeometryPointer(
        const GeometryDescriptor& desc, const std::string& name = "") override;
    IGroundPlanePtr createGroundPlanePointer() override;

    // TODO this is actually a bug. We should not hide IValues::type().
    IRenderer::Backend backend() const override { return IRenderer::Backend::HGI; }

    void setScene(const IScenePtr& pScene) override;
    void setTargets(const TargetAssignments& targetAssignments) override;
    void render(uint32_t sampleStart, uint32_t sampleCount) override;
    void waitForTask() override;
    void setLoadResourceFunction(LoadResourceFunction) override {}

    const std::vector<std::string>& builtInMaterials() override;
    pxr::HgiUniquePtr& hgi() { return _hgi; }
    const pxr::HgiBufferHandle& frameDataUbo() { return _frameDataUbo->handle(); }
    const pxr::HgiBufferHandle& sampleDataUbo() { return _sampleDataUbo->handle(); }

    HGIRenderBuffer* renderBuffer() { return _pRenderBuffer; }

    HGIScenePtr hgiScene() { return static_pointer_cast<HGIScene>(_pScene); }

    pxr::HgiTextureHandle directTex() { return _pDirectTex->handle(); }
    pxr::HgiTextureHandle accumulationTex() { return _pAccumulationTex->handle(); }

    pxr::HgiSamplerHandle sampler() { return _sampler->handle(); }

private:
    void createResources();
    pxr::HgiUniquePtr _hgi;
    HGIRenderBuffer* _pRenderBuffer;
    HgiBufferHandleWrapper::Pointer _frameDataUbo;
    HgiBufferHandleWrapper::Pointer _sampleDataUbo;
    HgiBufferHandleWrapper::Pointer _postProcessingUbo;
    HgiResourceBindingsHandleWrapper::Pointer _accumulationComputeResourceBindings;
    HgiComputePipelineHandleWrapper::Pointer _accumulationComputePipeline;
    HgiResourceBindingsHandleWrapper::Pointer _postProcessComputeResourceBindings;
    HgiComputePipelineHandleWrapper::Pointer _postProcessComputePipeline;
    HgiShaderProgramHandleWrapper::Pointer _accumComputeShaderProgram;
    HgiShaderProgramHandleWrapper::Pointer _postProcessComputeShaderProgram;
    HgiShaderFunctionHandleWrapper::Pointer _accumComputeShader;
    HgiShaderFunctionHandleWrapper::Pointer _postProcessComputeShader;

    HgiSamplerHandleWrapper::Pointer _sampler;
    HgiTextureHandleWrapper::Pointer _pDirectTex;
    HgiTextureHandleWrapper::Pointer _pAccumulationTex;

    MaterialShaderLibrary _shaderLibrary;
    shared_ptr<MaterialDefinition> _pDefaultMaterialDefinition;
    shared_ptr<MaterialShader> _pDefaultMaterialShader;
};

MAKE_AURORA_PTR(HGIRenderer);

END_AURORA
