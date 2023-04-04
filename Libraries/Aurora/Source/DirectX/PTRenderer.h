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

#include "PTEnvironment.h"
#include "PTGeometry.h"
#include "PTGroundPlane.h"
#include "PTMaterial.h"
#include "PTSampler.h"
#include "PTScene.h"
#include "PTTarget.h"
#include "RendererBase.h"

// Include the denoiser if it is enabled.
#if defined(ENABLE_DENOISER)
#include "Denoiser.h"
#endif

BEGIN_AURORA

namespace MaterialXCodeGen
{
class MaterialGenerator;
} // namespace MaterialXCodeGen

// Forward references.
class AssetManager;
class PTDevice;
class PTMaterialType;
class PTShaderLibrary;
class ScratchBufferPool;
class VertexBufferPool;

// An path tracing (PT) implementation for IRenderer.
class PTRenderer : public RendererBase
{
public:
    /*** Types ***/

    template <typename DataType>
    using FillDataFunction = function<void(DataType&)>;

    /*** Lifetime Management ***/

    /// Path tracing renderer constructor.
    ///
    /// \param activeTaskCount Maximum number of tasks active at once.
    PTRenderer(uint32_t activeTaskCount);
    ~PTRenderer();

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
        const GeometryDescriptor& desc, const string& name = "") override;
    IGroundPlanePtr createGroundPlanePointer() override;
    IRenderer::Backend backend() const override { return IRenderer::Backend::DirectX; }
    void setScene(const IScenePtr& pScene) override;
    void setTargets(const TargetAssignments& targetAssignments) override;
    void render(uint32_t sampleStart, uint32_t sampleCount) override;
    void waitForTask() override;
    const vector<string>& builtInMaterials() override;
    void setLoadResourceFunction(LoadResourceFunction func) override;

    /*** Functions ***/

    ID3D12Device5* dxDevice() const { return _pDXDevice.Get(); }
    IDXGIFactory4* dxFactory() const { return _pDXFactory.Get(); }
    ID3D12CommandQueue* commandQueue() const { return _pCommandQueue.Get(); }
    ID3D12ResourcePtr createBuffer(size_t size, const string& name = "",
        D3D12_HEAP_TYPE heapType    = D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAGS flags  = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ);
    template <typename DataType>
    void updateBuffer(ID3D12ResourcePtr& pBuffer, FillDataFunction<DataType> fillDataFunction);
    ID3D12ResourcePtr createTexture(uvec2 dimensions, DXGI_FORMAT format, const string& name = "",
        bool isUnorderedAccess = false, bool shareable = false);
    D3D12_GPU_VIRTUAL_ADDRESS getScratchBuffer(size_t size);
    void getVertexBuffer(VertexBuffer& vertexBuffer, void* pData);
    void flushVertexBufferPool();
    ID3D12CommandAllocator* getCommandAllocator();
    ID3D12GraphicsCommandList4* beginCommandList();
    void submitCommandList();
    void addTransitionBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);
    void addUAVBarrier(ID3D12Resource* pResource);
    void completeTask();
    PTSamplerPtr defaultSampler();
    PTGroundPlanePtr defaultGroundPlane();

private:
    /*** Private Types ***/
    static const size_t _FRAME_DATA_SIZE =
        ALIGNED_SIZE(sizeof(FrameData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    /*** Private Functions ***/

    bool initDevice();
    void initCommandList();
    void initFrameData();
    void initRayGenShaderTable();
    void initAccumulation();
    void initPostProcessing();
    void renderInternal(uint32_t sampleStart, uint32_t sampleCount);
    void updateRayGenShaderTable();
    void updateFrameData();
    void updateSceneResources();
    void updateOutputResources();
    void updateDenoisingResources();
    void prepareRayDispatch(D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc);
    void submitRayDispatch(const D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc, uint32_t sampleIndex,
        uint32_t seedOffset);
    void submitDenoising(bool isRestart);
    void submitAccumulation(uint32_t sampleIndex);
    void submitPostProcessing();
    void createUAV(ID3D12Resource* pTexture, CD3DX12_CPU_DESCRIPTOR_HANDLE& handle);
    void copyTextureToTarget(ID3D12Resource* pTexture, PTTarget* pTarget);
    bool isDenoisingAOVsEnabled() const;
    shared_ptr<PTMaterialType> generateMaterialX(
        const string& document, shared_ptr<MaterialDefinition>* pDefOut);
    PTScenePtr dxScene() { return static_pointer_cast<PTScene>(_pScene); }

    /*** Private Variables ***/

    unique_ptr<PTDevice> _pDevice;
    bool _isCommandListOpen = false;
    ID3D12ResourcePtr _pFrameDataBuffer;
    PTEnvironmentPtr _pEnvironment;
    uvec2 _outputDimensions;
    bool _isDimensionsChanged = true;
    uint32_t _seedOffset      = 0;
    PTTargetPtr _pTargetFinal;
    PTTargetPtr _pTargetDepthNDC;
    bool _isDescriptorHeapChanged = true;
    PTSamplerPtr _pDefaultSampler;
    PTGroundPlanePtr _pDefaultGroundPlane;

    // The shader library used to compile DXIL shader libraries.
    unique_ptr<PTShaderLibrary> _pShaderLibrary;

    // Code generator used to generate MaterialX files.
#if ENABLE_MATERIALX
    unique_ptr<MaterialXCodeGen::MaterialGenerator> _pMaterialXGenerator;
#endif

    /*** DirectX 12 Objects ***/

    ID3D12Device5Ptr _pDXDevice;
    IDXGIFactory4Ptr _pDXFactory;
    ID3D12CommandQueuePtr _pCommandQueue;
    vector<ID3D12CommandAllocatorPtr> _commandAllocators;
    ID3D12GraphicsCommandList4Ptr _pCommandList;
    ID3D12FencePtr _pTaskFence;
    HANDLE _hTaskEvent = nullptr;
    ID3D12PipelineStatePtr _pAccumulationPipelineState;
    ID3D12RootSignaturePtr _pAccumulationRootSignature;
    ID3D12PipelineStatePtr _pPostProcessingPipelineState;
    ID3D12RootSignaturePtr _pPostProcessingRootSignature;
    ID3D12DescriptorHeapPtr _pDescriptorHeap;
    ID3D12DescriptorHeapPtr _pSamplerDescriptorHeap;
    UINT _handleIncrementSize = 0;
    ID3D12ResourcePtr _pTexFinal;        // for tone-mapped final output (usually SDR)
    ID3D12ResourcePtr _pTexDepthNDC;     // for NDC depth output (usually float)
    ID3D12ResourcePtr _pTexAccumulation; // for accumulation (HDR)
    ID3D12ResourcePtr _pTexDirect;       // for path tracing or direct lighting (HDR)
    DXGI_FORMAT _finalFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    ID3D12ResourcePtr _pRayGenShaderTable;
    size_t _rayGenShaderTableSize = 0;
    unique_ptr<ScratchBufferPool> _pScratchBufferCache;
    unique_ptr<VertexBufferPool> _pVertexBufferPool;
    unordered_map<string, PTSamplerPtr> _mtlxSamplers;
    /*** Denoising Variables ***/

    // Include the denoiser if it is enabled. Other variables are still used for debug modes, even
    // when denoising itself is not enabled.
#if defined(ENABLE_DENOISER)
    unique_ptr<Denoiser> _pDenoiser;
#endif
    ID3D12ResourcePtr _pDenoisingTexDepthView;
    ID3D12ResourcePtr _pDenoisingTexNormalRoughness;
    ID3D12ResourcePtr _pDenoisingTexBaseColorMetalness;
    ID3D12ResourcePtr _pDenoisingTexDiffuse;
    ID3D12ResourcePtr _pDenoisingTexGlossy;
    ID3D12ResourcePtr _pDenoisingTexDiffuseOut;
    ID3D12ResourcePtr _pDenoisingTexGlossyOut;
};

// Creates (if needed) and an updates a GPU constant buffer with data supplied by the caller.
template <typename DataType>
void PTRenderer::updateBuffer(
    ID3D12ResourcePtr& pBuffer, FillDataFunction<DataType> fillDataFunction)
{
    // Create a constant buffer for the data if it doesn't already exist.
    static const size_t BUFFER_SIZE = sizeof(DataType);
    if (!pBuffer)
    {
        pBuffer = createBuffer(BUFFER_SIZE);
    }

    // Fill the data using the callback function.
    DataType data;
    fillDataFunction(data);

    // Copy the data to the constant buffer.
    void* pMappedData = nullptr;
    checkHR(pBuffer->Map(0, nullptr, &pMappedData));
    ::memcpy_s(pMappedData, BUFFER_SIZE, &data, BUFFER_SIZE);
    pBuffer->Unmap(0, nullptr); // no HRESULT
}

MAKE_AURORA_PTR(PTRenderer);

END_AURORA
