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

#include "PTRenderer.h"

#include "AssetManager.h"
#include "CompiledShaders/Accumulation.hlsl.h"
#include "CompiledShaders/PostProcessing.hlsl.h"
#include "MemoryPool.h"
#include "PTDevice.h"
#include "PTEnvironment.h"
#include "PTGeometry.h"
#include "PTGroundPlane.h"
#include "PTImage.h"
#include "PTLight.h"
#include "PTMaterial.h"
#include "PTScene.h"
#include "PTShaderLibrary.h"

#if ENABLE_MATERIALX
#include "MaterialX/MaterialGenerator.h"
#endif

// Include the denoiser if it is enabled.
#if defined(ENABLE_DENOISER)
#include "Denoiser.h"
#endif

// Development flag to dump materialX documents to disk.
// NOTE: This should never be enabled in committed code; it is only for local development.
#define AU_DEV_DUMP_MATERIALX_DOCUMENTS 0
#define AU_DEV_DUMP_PROCESSED_MATERIALX_DOCUMENTS 0

// Development flag to turn of exception catching during the render loop.
// NOTE: This should never be 0 in committed code; it is only for local development.
#define AU_DEV_CATCH_EXCEPTIONS_DURING_RENDERING 1

BEGIN_AURORA

// The number of descriptors in the descriptor heap used by the renderer.
// NOTE: This includes the output-related and denoising-related descriptors.
static const uint32_t kOutputDescriptorCount    = 4;
static const uint32_t kDenoisingDescriptorCount = 7;
static const uint32_t kDescriptorCount = kOutputDescriptorCount + kDenoisingDescriptorCount;

// The offsets of output-related descriptors.
static const int kFinalDescriptorOffset        = 0;
static const int kAccumulationDescriptorOffset = 1;
static const int kDirectDescriptorOffset       = 2;
static const int kDepthNDCDescriptorsOffset    = 3;

PTRenderer::PTRenderer(uint32_t taskCount) : RendererBase(taskCount)
{
    // Initialize a new device. If no suitable device can be created, return immediately.
    _isValid = initDevice();
    if (!_isValid)
    {
        return;
    }

    // Initialize the command list, which includes creating the command queue and a command
    // allocator for each simultaneously active task.
    initCommandList();

    // Initialize the shader library.
    // TODO: Should be per-scene not per-renderer.
    _pShaderLibrary = make_unique<PTShaderLibrary>(_pDXDevice);

    // Enable layer shaders.
    _pShaderLibrary->setOption("ENABLE_LAYERS", true);

#if ENABLE_MATERIALX
    // Get the materialX folder relative to the module path.
    string mtlxFolder = Foundation::getModulePath() + "MaterialX";
    // Initialize the MaterialX code generator.
    _pMaterialXGenerator = make_unique<MaterialXCodeGen::MaterialGenerator>(mtlxFolder);

    // Default to MaterialX distance unit to centimeters.
    _pShaderLibrary->setOption(
        "DISTANCE_UNIT", _pMaterialXGenerator->codeGenerator().units().indices.at("centimeter"));

    // Set the importance sampling mode.
    _pShaderLibrary->setOption("IMPORTANCE_SAMPLING_MODE", kImportanceSamplingModeMIS);
#endif

    // Initialize a per-frame data buffer.
    initFrameData();

    // Initialize the shader table.
    initRayGenShaderTable();

    // Initialize the accumulation and post-processing compute shaders.
    initAccumulation();
    initPostProcessing();

    // Initialize the scratch buffer and vertex buffer pools. The function to create scratch buffers
    // uses unordered access, as required for BLAS/TLAS scratch buffers.
    _pScratchBufferCache = make_unique<ScratchBufferPool>(_taskCount, [&](size_t size) {
        return createBuffer(size, "Scratch Buffer Pool", D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    });
    _pVertexBufferPool   = make_unique<VertexBufferPool>(
        [&](size_t size) { return createTransferBuffer(size, "VertexBufferPool"); });
}

PTRenderer::~PTRenderer()
{
    // Do nothing if the renderer is not valid, i.e. construction had failed.
    if (!_isValid)
    {
        return;
    }

    // Wait for the GPU to complete any pending work, so that resources are not released while they
    // are still being used.
    waitForTask();
}

// NOTE: In the createXXX() functions below, the new object has a raw pointer to the renderer, and
// must not exist beyond the lifetime of the renderer. This could be enforced with weak_ptr<>, but
// that is cumbersome.

IWindowPtr PTRenderer::createWindow(WindowHandle handle, uint32_t width, uint32_t height)
{
    // Create and return a new window object.
    return make_shared<PTWindow>(this, handle, width, height);
}

IRenderBufferPtr PTRenderer::createRenderBuffer(int width, int height, ImageFormat imageFormat)
{
    // Create and return a new render buffer object.
    return make_shared<PTRenderBuffer>(this, width, height, imageFormat);
}

IImagePtr PTRenderer::createImagePointer(const IImage::InitData& initData)
{
    // Create a return a new image object.
    return make_shared<PTImage>(this, initData);
}

ISamplerPtr PTRenderer::createSamplerPointer(const Properties& props)
{
    // Create a return a new sampler object.
    return make_shared<PTSampler>(this, props);
}

IMaterialPtr PTRenderer::createMaterialPointer(
    const string& materialType, const string& document, const string& name)
{
    // Validate material type.
    AU_ASSERT(materialType.compare(Names::MaterialTypes::kBuiltIn) == 0 ||
            materialType.compare(Names::MaterialTypes::kMaterialX) == 0 ||
            materialType.compare(Names::MaterialTypes::kMaterialXPath) == 0,
        "Invalid material type:", materialType.c_str());

    // Set the global "flipY" flag on the asset manager, to match option.
    // This has no overhead, so just do it each time.
    _pAssetMgr->enableVerticalFlipOnImageLoad(_values.asBoolean(kLabelIsFlipImageYEnabled));

    // The material shader and definition for this material.
    MaterialShaderPtr pShader;
    shared_ptr<MaterialDefinition> pDef;

    // Create a material type based on the material type name provided.
    if (materialType.compare(Names::MaterialTypes::kBuiltIn) == 0)
    {
        // Work out built-in type.
        string builtInType = document;

        // Get the built-in material type and definition for built-in.
        pShader = _pShaderLibrary->getBuiltInShader(builtInType);
        pDef    = _pShaderLibrary->getBuiltInMaterialDefinition(builtInType);

        // Print error and provide null material shader if built-in not found.
        // TODO: Proper error handling for this case.
        if (!pShader)
        {
            AU_ERROR("Unknown built-in material type %s for material %s", document.c_str(),
                name.c_str());
            pShader = nullptr;
        }
    }
    else if (materialType.compare(Names::MaterialTypes::kMaterialX) == 0)
    {
        // Generate a material shader and definition from the materialX document.
        pShader = generateMaterialX(document, &pDef);

        // If flag is set dump the document to disk for development purposes.
        if (AU_DEV_DUMP_MATERIALX_DOCUMENTS)
        {
            string mltxPath = name + "Dumped.mtlx";
            Foundation::sanitizeFileName(mltxPath);
            if (Foundation::writeStringToFile(document, mltxPath))
                AU_INFO("Dumping MTLX document to:%s", mltxPath.c_str());
            else
                AU_WARN("Failed to dump MTLX document to:%s", mltxPath.c_str());
        }
    }
    else if (materialType.compare(Names::MaterialTypes::kMaterialXPath) == 0)
    {
        // Load the MaterialX file using asset manager.
        auto pMtlxDocument = _pAssetMgr->acquireTextFile(document);

        // Print error and provide default material type if built-in not found.
        // TODO: Proper error handling for this case.
        if (!pMtlxDocument)
        {
            AU_ERROR("Failed to load MaterialX document %s for material %s", document.c_str(),
                name.c_str());
            pShader = nullptr;
        }
        else
        {
            // If Material XML document loaded, use it to generate the material shader and
            // definition.
            pShader = generateMaterialX(*pMtlxDocument, &pDef);
        }
    }
    else
    {
        // Print error and return null material shader if material type not found.
        // TODO: Proper error handling for this case.
        AU_ERROR(
            "Unrecognized material type %s for material %s.", materialType.c_str(), name.c_str());
        pShader = nullptr;
    }

    // Error case, just return null material.
    if (!pShader || !pDef)
        return nullptr;

    // Create the material object with the material shader and definition.
    auto pNewMtl = make_shared<PTMaterial>(this, pShader, pDef);

    // Set the default textures on the new material.
    for (int i = 0; i < pDef->defaults().textures.size(); i++)
    {
        auto txtDef = pDef->defaults().textures[i];

        // Image default values are provided as strings and must be loaded.
        auto textureFilename = txtDef.defaultFilename;
        if (!textureFilename.empty())
        {
            // Load the pixels for the image using asset manager.
            auto pImageData = _pAssetMgr->acquireImage(textureFilename);
            if (!pImageData)
            {
                // Print error if image fails to load, and then ignore default.
                // TODO: Proper error handling here.
                AU_ERROR("Failed to load image data %s for material %s", textureFilename.c_str(),
                    name.c_str());
            }
            else
            {
                // Set the linearize flag.
                // TODO: Should effect caching.
                pImageData->data.linearize = txtDef.linearize;

                // Create Ultra image from the loaded pixels.
                // TODO: This should be cached by filename.
                auto pImage = createImagePointer(pImageData->data);
                if (!pImage)
                {
                    // Print error if image creation fails, and then ignore default.
                    // TODO: Proper error handling here.
                    AU_ERROR("Failed to create image %s for material %s", textureFilename.c_str(),
                        name.c_str());
                }
                else
                {
                    // Set the default image.
                    pNewMtl->setImage(txtDef.name, pImage);
                }
            }
        }

        // If we have an address mode, create a sampler for texture.
        // Currently only the first two hardcoded textures have samplers, so only do this for first
        // two textures.
        // TODO: Move to fully data driven textures and samplers.
        if (i < 2 && (!txtDef.addressModeU.empty() || !txtDef.addressModeV.empty()))
        {
            Properties samplerProps;

            // Set U address mode.
            if (txtDef.addressModeU.compare("periodic") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeU] = Names::AddressModes::kWrap;
            else if (txtDef.addressModeU.compare("clamp") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeU] = Names::AddressModes::kClamp;
            else if (txtDef.addressModeU.compare("mirror") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeU] =
                    Names::AddressModes::kMirror;

            // Set V address mode.
            if (txtDef.addressModeV.compare("periodic") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeV] = Names::AddressModes::kWrap;
            else if (txtDef.addressModeV.compare("clamp") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeV] = Names::AddressModes::kClamp;
            else if (txtDef.addressModeV.compare("mirror") == 0)
                samplerProps[Names::SamplerProperties::kAddressModeV] =
                    Names::AddressModes::kMirror;

            // Create a sampler and set in the material.
            // TODO: Don't assume hardcoded _sampler prefix.
            auto pSampler = createSamplerPointer(samplerProps);
            pNewMtl->setSampler(txtDef.name + "_sampler", pSampler);
        }
    }

    // Return new material.
    return pNewMtl;
}

IScenePtr PTRenderer::createScene()
{
    // Create and return a new scene object.
    return make_shared<PTScene>(this, _pShaderLibrary.get(), kDescriptorCount);
}

IEnvironmentPtr PTRenderer::createEnvironmentPointer()
{
    // Create and return a new environment object.
    return make_shared<PTEnvironment>(this);
}

IGeometryPtr PTRenderer::createGeometryPointer(const GeometryDescriptor& desc, const string& name)
{
    // Create and return a new environment object.
    return make_shared<PTGeometry>(this, name, desc);
}

IGroundPlanePtr PTRenderer::createGroundPlanePointer()
{
    // Create and return a new ground plane object.
    return make_shared<PTGroundPlane>(this);
}

void PTRenderer::setScene(const IScenePtr& pScene)
{
    // Wait for the GPU to complete any pending work, so that scene resources are not released
    // while they are still being used.
    if (_pScene)
    {
        waitForTask();
    }

    // Assign the new scene.
    _pScene = dynamic_pointer_cast<SceneBase>(pScene);
    ;
}

void PTRenderer::setTargets(const TargetAssignments& targetAssignments)
{
    // TODO: Validate that all targets have the same dimensions. Will involve adding a dimensions
    // accessor to ITarget.
    AU_ASSERT(targetAssignments.size() > 0, "There must be at least one target assignment.");

    // Get the target assigned to the final AOV (required).
    auto targetIt = targetAssignments.find(AOV::kFinal);
    AU_ASSERT(targetIt != targetAssignments.end(), "A target must be assigned to the Final AOV.");
    _pTargetFinal = dynamic_pointer_cast<PTTarget>(targetIt->second);

    // Get the target assigned to the NDC depth AOV (optional).
    targetIt         = targetAssignments.find(AOV::kDepthNDC);
    _pTargetDepthNDC = targetIt != targetAssignments.end()
        ? dynamic_pointer_cast<PTTarget>(targetIt->second)
        : nullptr;
}

void PTRenderer::render(uint32_t sampleStart, uint32_t sampleCount)
{
    // This function is a major entry point, so issue a fatal error if the renderer is not valid. It
    // will not be valid if initialization failed, or if there was a fatal error during rendering.
    // NOTE: The error handling further below is not sufficient to handle this because attempting to
    // use an invalid renderer could result in a memory access violation, which is not covered by
    // the try / catch block. Handling that requires platform-specific support, which is not worth
    // doing here.
    if (!_isValid)
    {
        AU_FAIL("Attempting to render with an invalid renderer.");
    }

    // Perform rendering. This includes exception handling to catch any fatal errors that may occur
    // in rendering.
    //
    // Specifically, this is meant to catch timeout detection and recovery (TDR) events, when a
    // single operation takes longer than two seconds (by default). This can happen with path
    // tracing on a slow GPU, or with large output dimensions, or with a complex scene. When this
    // happens, the device is removed ("lost") and resources must be recreated. However, Aurora does
    // not retain enough information for such a recovery, so this is treated as a fatal
    // (catastrophic) error and AU_FAIL is used to indicate this.
    //
    // By default, AU_FAIL will abort the application, so if the application wants to attempt
    // recovery, it must set a callback on the Log interface. Note that the Aurora renderer cannot
    // be reused after a failure, so it should be released and recreated.
#if AU_DEV_CATCH_EXCEPTIONS_DURING_RENDERING
    try
    {

#endif
        renderInternal(sampleStart, sampleCount);

#if AU_DEV_CATCH_EXCEPTIONS_DURING_RENDERING
    }
    catch (...)
    {
        _isValid = false;
        AU_FAIL("Rendering has failed, likely due to an operation taking too long to complete.");
    }
#endif
}

void PTRenderer::waitForTask()
{
    // NOTE: This causes the CPU to wait for all pending GPU work to finish. This is generally used
    // to avoid resource contention. However, this can affect performance should be avoided if
    // possible.

    // Set an event to be triggered when the fence reaches the *previous* task number, and wait
    // until that event is triggered. This means the the work for the previous task is done.
    // NOTE: The very first task is skipped, as there is no prior task to wait for. Also the fence
    // value must start at one, as zero is not valid, so one (1) is added here.
    if (_taskNumber > 0)
    {
        uint64_t fenceValue = _taskNumber - 1;
        checkHR(_pTaskFence->SetEventOnCompletion(fenceValue + 1, _hTaskEvent));
        ::WaitForSingleObject(_hTaskEvent, INFINITE);
    }
}

const vector<string>& PTRenderer::builtInMaterials()
{
    return _pShaderLibrary->builtInMaterials();
}

void PTRenderer::setLoadResourceFunction(LoadResourceFunction func)
{
    _pAssetMgr->setLoadResourceFunction(func);
}

ID3D12ResourcePtr PTRenderer::createBuffer(size_t size, const string& name,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state)
{
    // Specify the heap properties and buffer description.
    CD3DX12_HEAP_PROPERTIES heapProps(heapType);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

    // Create a committed resource of the specified size.
    ID3D12ResourcePtr pResource;
    checkHR(_pDXDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, state, nullptr, IID_PPV_ARGS(&pResource)));

    // Set the resource name.
    checkHR(pResource->SetName(Foundation::s2w(name).c_str()));

    return pResource;
}

TransferBuffer PTRenderer::createTransferBuffer(size_t sz, const string& name,
    D3D12_RESOURCE_FLAGS gpuBufferFlags, D3D12_RESOURCE_STATES gpuBufferState,
    D3D12_RESOURCE_STATES gpuBufferFinalState)
{
    TransferBuffer buffer;
    // Create the upload buffer in the UPLOAD heap (which will mean it is in CPU memory not VRAM).
    buffer.pUploadBuffer = createBuffer(sz, name + ":Upload");
    // Create the GPU buffer in the DEFAULT heap, with the flags and state provided (these will
    // default to D3D12_RESOURCE_FLAG_NONE and D3D12_RESOURCE_STATE_COPY_DEST).
    buffer.pGPUBuffer =
        createBuffer(sz, name + ":GPU", D3D12_HEAP_TYPE_DEFAULT, gpuBufferFlags, gpuBufferState);
    // Set the size.
    buffer.size = sz;
    // Set the renderer to this (will call transferBufferUpdated from unmap.)
    buffer.pRenderer = this;
    // Set the final state for the buffer, which it will transition to after uploading.
    buffer.finalState = gpuBufferFinalState;
    return buffer;
}

ID3D12ResourcePtr PTRenderer::createTexture(uvec2 dimensions, DXGI_FORMAT format,
    const string& name, bool isUnorderedAccess, bool shareable)
{
    // Prepare a texture description.
    D3D12_RESOURCE_FLAGS resourceFlag =
        isUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    CD3DX12_RESOURCE_DESC texDesc =
        CD3DX12_RESOURCE_DESC::Tex2D(format, dimensions.x, dimensions.y, 1, 1, 1, 0, resourceFlag);

    // Set the initial resource state to "copy" or unordered access, because that is what the render
    // process expects as the state.
    D3D12_RESOURCE_STATES resourceState =
        isUnorderedAccess ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COPY_DEST;

    // Set the appropriate flags for sharing the texture across devices, if requested.
    D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
    if (shareable)
    {
        heapFlags = D3D12_HEAP_FLAG_SHARED;
        texDesc.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    }

    // Create a resource using the description and flags.
    ID3D12ResourcePtr pResource;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    checkHR(_pDXDevice->CreateCommittedResource(
        &heapProps, heapFlags, &texDesc, resourceState, nullptr, IID_PPV_ARGS(&pResource)));

    // Set the resource name.
    checkHR(pResource->SetName(Foundation::s2w(name).c_str()));

    return pResource;
}

D3D12_GPU_VIRTUAL_ADDRESS PTRenderer::getScratchBuffer(size_t size)
{
    return _pScratchBufferCache->get(size);
}

void PTRenderer::transferBufferUpdated(const TransferBuffer& buffer)
{
    // Get the mapped range for buffer (set the end to buffer size, in the case where end==0.)
    size_t beginMap = buffer.mappedRange.Begin;
    size_t endMap   = buffer.mappedRange.End == 0 ? buffer.size : buffer.mappedRange.End;

    // See if this buffer already exists in pending list.
    auto iter = _pendingTransferBuffers.find(buffer.pGPUBuffer.Get());
    if (iter != _pendingTransferBuffers.end())
    {
        // Just update the mapped range in the existing pending buffer.
        iter->second.mappedRange.Begin = std::min(iter->second.mappedRange.Begin, beginMap);
        iter->second.mappedRange.End   = std::max(iter->second.mappedRange.End, endMap);
        return;
    }

    // Add the buffer in the pending list, updating the end range if needed.
    _pendingTransferBuffers[buffer.pGPUBuffer.Get()]                 = buffer;
    _pendingTransferBuffers[buffer.pGPUBuffer.Get()].mappedRange.End = endMap;
}

void PTRenderer::getVertexBuffer(VertexBuffer& vertexBuffer, void* pData, size_t size)
{
    _pVertexBufferPool->get(vertexBuffer, pData, size);
}

void PTRenderer::flushVertexBufferPool()
{
    _pVertexBufferPool->flush();
}

void PTRenderer::deleteUploadedTransferBuffers()
{
    // Do nothing if no buffers to delete.
    if (_transferBuffersToDelete.empty())
        return;

    // Clear the list, so any upload or GPU buffer without a reference to it will be deleted.
    _transferBuffersToDelete.clear();
}

void PTRenderer::uploadTransferBuffers()
{
    // If there are no transfer buffers pending, do nothing.
    if (_pendingTransferBuffers.empty())
        return;

    // Begin a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = beginCommandList();

    // Iterate through all pending buffers.
    for (auto iter = _pendingTransferBuffers.begin(); iter != _pendingTransferBuffers.end(); iter++)
    {
        // Get pending buffer.
        auto& buffer = iter->second;

        // Calculate the byte count from the mapped range (which will be the maximum range mapped
        // this frame.)
        size_t bytesToCopy = buffer.mappedRange.End - buffer.mappedRange.Begin;

        // If this buffer was previously uploaded we need transition back to
        // D3D12_RESOURCE_STATE_COPY_DEST before copying.
        if (buffer.wasUploaded)
            addTransitionBarrier(
                buffer.pGPUBuffer.Get(), buffer.finalState, D3D12_RESOURCE_STATE_COPY_DEST);

        // Submit buffer copy command for mapped range from the upload buffer to the GPU buffer.
        pCommandList->CopyBufferRegion(buffer.pGPUBuffer.Get(), buffer.mappedRange.Begin,
            buffer.pUploadBuffer.Get(), buffer.mappedRange.Begin, bytesToCopy);

        // Transition the buffer to its final state.
        // Note in practice the Nvidia driver seems to do this implicitly without any problems,
        // though the spec says this explicit transition is required.
        addTransitionBarrier(
            buffer.pGPUBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, buffer.finalState);

        // Set the uploaded flag.
        buffer.wasUploaded = true;

        // Add the buffer to the list to be deleted (if nothing has kept a reference to it) next
        // frame.
        _transferBuffersToDelete[iter->first] = iter->second;
    }

    // Submit the command list.
    submitCommandList();

    // Clear the pending list.
    _pendingTransferBuffers.clear();
}

ID3D12CommandAllocator* PTRenderer::getCommandAllocator()
{
    return _commandAllocators[_taskIndex].Get();
}

ID3D12GraphicsCommandList4* PTRenderer::beginCommandList()
{
    assert(!_isCommandListOpen);
    _isCommandListOpen = true;

    // Reset the command list using the current command allocator.
    // NOTE: It is safe to do this even if commands that were created with the command list are
    // still being executed. It is the command *allocator* that can't be reset that way.
    checkHR(_pCommandList->Reset(_commandAllocators[_taskIndex].Get(), nullptr));

    return _pCommandList.Get();
}

void PTRenderer::submitCommandList()
{
    assert(_isCommandListOpen);
    _isCommandListOpen = false;

    // Close the command list and execute it on the command queue.
    checkHR(_pCommandList->Close());
    ID3D12CommandList* pCommandList = _pCommandList.Get();
    _pCommandQueue->ExecuteCommandLists(1, &pCommandList); // no HRESULT
}

void PTRenderer::addTransitionBarrier(
    ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
    assert(_isCommandListOpen);

    // Insert a resource barrier into the command list to transition the resource from its previous
    // state to a new one.
    CD3DX12_RESOURCE_BARRIER barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(pResource, stateBefore, stateAfter);
    _pCommandList->ResourceBarrier(1, &barrier); // no HRESULT
}

void PTRenderer::addUAVBarrier(ID3D12Resource* pResource)
{
    assert(_isCommandListOpen);

    // Insert a resource barrier to ensure all UAV reads / writes are completed.
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(pResource);
    _pCommandList->ResourceBarrier(1, &barrier); // no HRESULT
}

void PTRenderer::completeTask()
{
    assert(!_isCommandListOpen);

    // NOTE: A "task" manages simultaneous access to limited resources. Specifically this applies
    // to the following resources:
    // - The command allocators: these must be reset regularly to avoid excessive memory
    //   consumption, so they are reset (arbitrarily) after each "task" of work. They can't be
    //   reset while they have commands that have been queued or are executing.
    // - The per-frame constant buffer: this is a single buffer partitioned into copies of data for
    //   each possible task queued for rendering. These are updated by the CPU, but this must not
    //   be done while the GPU might be reading the data.
    //
    // There are three relevant members:
    // - _taskCount: The number of simultaneously active tasks (default 3). Making this larger
    //   increases latency, but smaller may reduce performance.
    // - _taskIndex: Which of the available task "slots" is currently being used for new work, as a
    //   value between zero and _taskCount.
    // - _taskNumber: The current overall task number, a value that increments by one for each
    //   completed task. This does not necessarily correspond to "frames."

    // Have the command queue update the fence to the current task number plus one, when all pending
    // command lists have been processed.
    // NOTE: Fence value is always taskNumber+1 as zero is not a valid fence value.
    checkHR(_pCommandQueue->Signal(_pTaskFence.Get(), (_taskNumber + 1)));

    // Increment the task number.
    _taskNumber++;

    // Wait for the work for the task that last used the data for the current task index. For
    // example, if there are three active tasks, and this is overall task #4, then we need to make
    // sure that the work for task #1 (three tasks ago) is complete. This is "triple buffering."
    // NOTE: The fence value must start at one, as zero is not valid, so one (1) is added here.
    if (_taskNumber > _taskCount)
    {
        uint64_t fenceValue = _taskNumber - _taskCount;
        checkHR(_pTaskFence->SetEventOnCompletion(fenceValue + 1, _hTaskEvent));
        ::WaitForSingleObject(_hTaskEvent, INFINITE);
    }

    // Determine the current task index, which repeats: 0, 1, 2, 0, 1, etc. Reset the corresponding
    // command allocator, and reset the command list using that command allocator.
    _taskIndex = _taskNumber % _taskCount;
    checkHR(_commandAllocators[_taskIndex]->Reset());

    // Update the scratch buffer cache with the new task index.
    _pScratchBufferCache->update(_taskIndex);
}

PTSamplerPtr PTRenderer::defaultSampler()
{
    // Create a default sampler if it does not already exist.
    // NOTE: The object has a raw pointer to the renderer, and must not exist beyond the lifetime of
    // the renderer. This could be enforced with weak_ptr<>, but that is cumbersome.
    if (!_pDefaultSampler)
    {
        _pDefaultSampler = dynamic_pointer_cast<PTSampler>(createSamplerPointer({}));
    }

    // Return the existing default sampler.
    return _pDefaultSampler;
}

PTGroundPlanePtr PTRenderer::defaultGroundPlane()
{
    // Create a default ground plane if it does not already exist.
    // NOTE: The object has a raw pointer to the renderer, and must not exist beyond the lifetime of
    // the renderer. This could be enforced with weak_ptr<>, but that is cumbersome.
    if (!_pDefaultGroundPlane)
    {
        _pDefaultGroundPlane = dynamic_pointer_cast<PTGroundPlane>(createGroundPlanePointer());

        // Set the default ground plane as disabled, so that setting a null ground plane on a scene
        // will remove (i.e. not render) the ground plane.
        _pDefaultGroundPlane->values().setBoolean("enabled", false);
    }

    return _pDefaultGroundPlane;
}

bool PTRenderer::initDevice()
{
    // Create a device with ray tracing support.
    _pDevice = PTDevice::create(PTDevice::Features::kRayTracing);
    if (!_pDevice)
    {
        return false;
    }

    // Retain the DirectX device and factory objects.
    _pDXDevice  = _pDevice->device();
    _pDXFactory = _pDevice->factory();

    // Store the descriptor heap handle increment size for future use.
    _handleIncrementSize =
        _pDXDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return true;
}

void PTRenderer::initCommandList()
{
    // Create a command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    checkHR(_pDXDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_pCommandQueue)));

    // Create a command allocator for each active task, as a command allocator is needed for each
    // possible command list that is queued at once.
    _commandAllocators.resize(_taskCount);
    for (uint32_t i = 0; i < _taskCount; i++)
    {
        checkHR(_pDXDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&(_commandAllocators[i]))));
    }

    // Create a command list. Only a single command list is required for each thread, just one here.
    // NOTE: It is initialized with the first command allocator but will be reset with the command
    // allocator for the current active task index.
    checkHR(_pDXDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        _commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&_pCommandList)));
    _pCommandList->Close();

    // Create a fence and event for detecting when a command list is complete.
    checkHR(_pDXDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_pTaskFence)));
    _hTaskEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void PTRenderer::initFrameData()
{
    // Create a buffer to store frame data for each buffer. A single buffer can be used for this,
    // as long as each section of the buffer is not written to while it is being used by the GPU.
    size_t frameDataBufferSize = _FRAME_DATA_SIZE * _taskCount;
    _frameDataBuffer           = createTransferBuffer(frameDataBufferSize, "FrameData");
}

void PTRenderer::initRayGenShaderTable()
{
    // Compute the stride of each record in the shader table. The stride includes the shader
    // identifier and its parameters. Also compute the total size of the table.
    size_t rayGenShaderRecordStride = SHADER_ID_SIZE;
    rayGenShaderRecordStride += SHADER_RECORD_DESCRIPTOR_SIZE; // for the descriptor table
    rayGenShaderRecordStride = ALIGNED_SIZE(rayGenShaderRecordStride, SHADER_RECORD_ALIGNMENT);
    _rayGenShaderTableSize   = rayGenShaderRecordStride; // just one shader record

    // Create a transfer buffer for the shader table.
    _rayGenShaderTable = createTransferBuffer(_rayGenShaderTableSize, "RayGenShaderTable");
}

void PTRenderer::updateRayGenShaderTable()
{
    // Map the ray gen shader table.
    uint8_t* pShaderTableMappedData = _rayGenShaderTable.map();

    // Write the shader identifier for the ray gen shader.
    ::memcpy_s(pShaderTableMappedData, _rayGenShaderTableSize,
        _pShaderLibrary->getSharedEntryPointShaderID(EntryPointTypes::kRayGen), SHADER_ID_SIZE);

    // Unmap the shader table buffer.
    _rayGenShaderTable.unmap();
}

void PTRenderer::initAccumulation()
{
    // Get the byte code for the compute shader.
    D3D12_SHADER_BYTECODE shaderByteCode =
        CD3DX12_SHADER_BYTECODE(g_pAccumulationShader, _countof(g_pAccumulationShader));

    // Create a root signature from the definition in the byte code.
    checkHR(_pDXDevice->CreateRootSignature(0, g_pAccumulationShader,
        _countof(g_pAccumulationShader), IID_PPV_ARGS(&_pAccumulationRootSignature)));

    // Prepare compute pipeline state with the compute shader.
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS                                = shaderByteCode;
    checkHR(
        _pDXDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&_pAccumulationPipelineState)));
}

void PTRenderer::initPostProcessing()
{
    // Get the byte code for the compute shader.
    D3D12_SHADER_BYTECODE shaderByteCode =
        CD3DX12_SHADER_BYTECODE(g_pPostProcessingShader, _countof(g_pPostProcessingShader));

    // Create a root signature from the definition in the byte code.
    checkHR(_pDXDevice->CreateRootSignature(0, g_pPostProcessingShader,
        _countof(g_pPostProcessingShader), IID_PPV_ARGS(&_pPostProcessingRootSignature)));

    // Prepare compute pipeline state with the compute shader.
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.CS                                = shaderByteCode;
    checkHR(_pDXDevice->CreateComputePipelineState(
        &desc, IID_PPV_ARGS(&_pPostProcessingPipelineState)));
}

void PTRenderer::renderInternal(uint32_t sampleStart, uint32_t sampleCount)
{
    assert(_pScene && _pTargetFinal);
    assert(sampleCount > 0);

    // If non-zero instance count, then ensure we have valid bounds.
    AU_ASSERT(!dxScene()->instanceCount() || dxScene()->bounds().isValid(),
        "Scene bounds are not valid. Were valid bounds set with IScene::setBounds() ?");

    // Retain certain option state for the frame.
    bool isResetHistoryEnabled = _values.asBoolean(kLabelIsResetHistoryEnabled);

    // Call preUpdate function (will create any resources before the scene and shaders are rebuilt).
    dxScene()->preUpdate();

    // Have any options changed?
    if (_bIsDirty)
    {
#if ENABLE_MATERIALX
        // Get the units option.
        string unit = _values.asString(kLabelUnits);
        // Lookup the unit in the code generator, and ensure it is valid.
        auto unitIter = _pMaterialXGenerator->codeGenerator().units().indices.find(unit);
        if (unitIter == _pMaterialXGenerator->codeGenerator().units().indices.end())
        {
            AU_ERROR("Invalid unit:" + unit);
        }
        else
        {
            // Set the option in the shader library.
            _pShaderLibrary->setOption("DISTANCE_UNIT", unitIter->second);
        }
#endif

        // Set the importance sampling mode.
        _pShaderLibrary->setOption(
            "IMPORTANCE_SAMPLING_MODE", _values.asInt(kLabelImportanceSamplingMode));

        // Set the default BSDF (if true will use Reference, if false will use Standard Surface.)
        _pShaderLibrary->setOption(
            "USE_REFERENCE_BSDF", _values.asBoolean(kLabelIsReferenceBSDFEnabled));

        // Clear the dirty flag.
        _bIsDirty = false;
    }

    // Rebuild shader library if needed.
    if (_pShaderLibrary->rebuildRequired())
    {
        // If shader rebuild required, wait for GPU to be idle, then rebuild.
        waitForTask();
        _pShaderLibrary->rebuild();

        // Update the ray gen shader table after rebuild.
        updateRayGenShaderTable();

        // Clear the scene's shader data, to ensure that is rebuilt too.
        dxScene()->clearShaderData();
    }

    // Update the scene.
    dxScene()->update();

    // Update the resources associated with the scene, output (targets), and denoising.
    // NOTE: These updates are very fast if nothing relevant has changed since the last render.
    _isDimensionsChanged     = _outputDimensions != _pTargetFinal->dimensions();
    _outputDimensions        = _pTargetFinal->dimensions();
    _isDescriptorHeapChanged = _pDescriptorHeap.Get() != dxScene()->descriptorHeap();
    _pDescriptorHeap         = dxScene()->descriptorHeap();
    _pSamplerDescriptorHeap  = dxScene()->samplerDescriptorHeap();
    updateSceneResources();
    updateOutputResources();
    updateDenoisingResources();
    _isDimensionsChanged     = false;
    _isDescriptorHeapChanged = false;

    // Use an incrementing (but gradually repeating) random number seed offset when temporal
    // accumulation is needed, for denoising. This offset ensures that the same set of random
    // numbers is not reused frame-to-frame, which would prevent proper temporal accumulation. For
    // temporal accumulation is not needed (not denoising), do not use an offset, to avoid obvious
    // moving "static" between frames.
    uint32_t seedOffset = 0;
    if (_values.asBoolean(kLabelIsDenoisingEnabled))
    {
        // Increment the stored seed offset when rendering has been restarted.
        if (sampleStart == 0)
        {
            constexpr uint32_t kRepeat = 100;
            _seedOffset                = ++_seedOffset % kRepeat;
        }

        // Use the stored seed offset, instead of the default zero.
        seedOffset = _seedOffset;
    }

    // Prepare a ray dispatch description, and perform a ray tracing dispatch for each requested
    // sample, starting at the sample start index.
    // NOTE: While it is possible to render multiple samples in a single dispatch instead of using a
    // loop (as done here), that is actually much slower in practice.
    D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};
    prepareRayDispatch(dispatchRaysDesc);
    for (uint32_t i = 0; i < sampleCount; i++)
    {
        // Update the per-frame data. There is one copy of the data per task, and a task is
        // completed at the end of this loop, so the data update must be performed here.
        updateFrameData();

        // Perform ray tracing for the current sample.
        submitRayDispatch(dispatchRaysDesc, sampleStart + i, seedOffset);

        // Perform denoising of the diffuse / glossy radiance results.
        // NOTE: This does nothing if denoising is not enabled.
        submitDenoising(isResetHistoryEnabled);

        // Perform accumulation using the generated AOVs, some of which may have been denoised.
        // NOTE: This should not use the sample offset, as it needs to know how far along the
        // accumulation has proceeded.
        submitAccumulation(sampleStart + i);

        // Complete the task here, because the denoiser itself only supports the same number of
        // tasks and uses one for each sample counter increment.
        completeTask();
    }

    // Perform post-processing and copy the results to the targets.
    submitPostProcessing();

    // Present the targets.
    // NOTE: If the target has a swap chain, the following applies to avoid errors:
    // - WRONGSWAPCHAINBUFFERREFERENCE: Present must be done *after* the previous command list is
    //   submitted to avoid this immediate error.
    // - OBJECT_DELETED_WHILE_STILL_IN_USE: Present must be done *before* ending the task, to avoid
    //   this error on swap chain resize or application shutdown. Specifically, a command queue
    //   Signal() call must be made after presenting. The reason for this is unclear, but it has to
    //   do with the swap chain interacting with the command queue it was created with.
    _pTargetFinal->present();
    if (_pTargetDepthNDC)
    {
        _pTargetDepthNDC->present();
    }

    // Complete the task, so that rendering is ready for the next task.
    completeTask();

    // Clear the history reset flag.
    // NOTE: This is done as a convenience for the client, as resetting history is almost always
    // intended only for one frame.
    _values.setValue(kLabelIsResetHistoryEnabled, false);
}

void PTRenderer::updateFrameData()
{
    // Call base-class update function to get GPU frame data.
    updateFrameDataGPUStruct();

    // Get other options. Limit the trace depth to the range [1, kMaxTraceDepth].
    // TODO: Should be in base class too.
    _frameData.isDepthNDCEnabled      = _pTargetDepthNDC ? 1 : 0;
    _frameData.isDenoisingAOVsEnabled = isDenoisingAOVsEnabled() ? 1 : 0;

    // Copy frame data to the frame data buffer, at the buffer data location for the current task
    // index. The updated section of the buffer must be set on the pipeline later with
    // SetComputeRootConstantBufferView().
    uint8_t* pFrameDataBufferMappedData = nullptr;
    size_t start                        = _FRAME_DATA_SIZE * _taskIndex;
    size_t end                          = start + _FRAME_DATA_SIZE;
    pFrameDataBufferMappedData          = _frameDataBuffer.map(end, start);
    ::memcpy_s(
        pFrameDataBufferMappedData + start, _FRAME_DATA_SIZE, &_frameData, sizeof(_frameData));
    _frameDataBuffer.unmap();

    // Upload the transfer buffers after changing the frame data, so the new frame data is available
    // on the GPU while rendering frame.
    uploadTransferBuffers();
}

void PTRenderer::updateSceneResources()
{
    // Do nothing if the descriptor heap is unchanged.
    // NOTE: A changing descriptor heap also corresponds with an entirely new scene.
    if (!_isDescriptorHeapChanged)
    {
        return;
    }

    // Set the descriptor heap on the ray gen shader table, for descriptors needed from the heap for
    // the ray gen shader. The ray gen shader expects the first entry to be the direct texture.
    uint8_t* pShaderTableMappedData = _rayGenShaderTable.map();
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        kDirectDescriptorOffset, _handleIncrementSize);
    ::memcpy_s(pShaderTableMappedData + SHADER_ID_SIZE, _rayGenShaderTableSize, &handle,
        SHADER_RECORD_DESCRIPTOR_SIZE);
    _rayGenShaderTable.unmap();
}

void PTRenderer::updateOutputResources()
{
    // NOTE: If the client switches between several resident targets with different dimensions (e.g.
    // several viewports), creating new output textures every time may become a performance
    // bottleneck. This can be resolved by having a small cache of output textures with recently
    // used dimensions.
    //
    // Only single textures are required (not one per active task), as the renderer does not employ
    // multiple command queues that could operate simultaneously, and the CPU does not read or write
    // these textures.

    // Get the final target image format, converted to a DXGI format.
    DXGI_FORMAT newFormat = PTImage::getDXFormat(_pTargetFinal->format());

    // Create new required textures if the output dimensions or image format have changed.
    bool isTexturesUpdated = false;
    if (_isDimensionsChanged || _finalFormat != newFormat)
    {
        // If there is an existing final texture, flush the renderer to make sure the related
        // textures are no longer being used by the pipeline. NOTE: It would be redundant to also
        // check the other textures, so that is not done.
        if (_pTexFinal)
        {
            waitForTask();
        }

        // Create the final texture with the final target format (usually integer SDR), and the
        // accumulation and direct textures with a floating-point (HDR) format, and all with
        // unordered access.
        // NOTE: A texture with unordered access can't have an "_SRGB" format, so gamma correction
        // must be performed in the post-processing step.
        const DXGI_FORMAT kHDRFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
        _finalFormat                 = newFormat;
        _pTexFinal        = createTexture(_outputDimensions, _finalFormat, "Output", true);
        _pTexAccumulation = createTexture(_outputDimensions, kHDRFormat, "HDR Accumulation", true);
        _pTexDirect       = createTexture(_outputDimensions, kHDRFormat, "HDR Direct", true);
        isTexturesUpdated = true;
    }

    // Create (or clear) the optional NDC depth texture if the output dimensions have changed or the
    // target has been toggled (set or removed).
    if (_isDimensionsChanged ||
        static_cast<bool>(_pTargetDepthNDC) != static_cast<bool>(_pTexDepthNDC))
    {
        // If there is an existing NDC depth texture, flush the renderer to make sure the texture is
        // no longer being used by the pipeline.
        if (_pTexDepthNDC)
        {
            waitForTask();
        }

        // Create or clear the NDC depth texture.
        if (_pTargetDepthNDC)
        {
            const DXGI_FORMAT kDepthNDCFormat = DXGI_FORMAT_R32_FLOAT;
            _pTexDepthNDC = createTexture(_outputDimensions, kDepthNDCFormat, "NDC Depth", true);
        }
        else
        {
            _pTexDepthNDC.Reset();
        }
        isTexturesUpdated = true;
    }

    // If textures have been updated, or the descriptor heap has changed, create UAVs for the
    // textures.
    if (isTexturesUpdated || _isDescriptorHeapChanged)
    {
        // Flush the renderer to make sure the (unchanged) descriptor heap is not being used.
        if (!_isDescriptorHeapChanged)
        {
            waitForTask();
        }

        // Create UAVs for the textures as the first entries in the descriptor heap.
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
            _pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        createUAV(_pTexFinal.Get(), handle);
        createUAV(_pTexAccumulation.Get(), handle);
        createUAV(_pTexDirect.Get(), handle);
        createUAV(_pTexDepthNDC.Get(), handle);
    }
}

void PTRenderer::updateDenoisingResources()
{
    // Create or destroy denoising resources as needed.
    bool isTexturesUpdated = false;
    if (isDenoisingAOVsEnabled())
    {
        // If the dimensions have changed, or there are no existing denoising resources,
        // recreate the denoising resources.
        if (_isDimensionsChanged || !_pDenoisingTexDepthView)
        {
            isTexturesUpdated = true;

            // If there is any existing denoising resource (using one to represent all of them),
            // flush the renderer to make sure the denoising resources are no longer being used
            // by the pipeline.
            if (_pDenoisingTexDepthView)
            {
                waitForTask();
            }

            // Define resource formats for denoising textures.
            const DXGI_FORMAT kDepthViewFormat          = DXGI_FORMAT_R32_FLOAT;
            const DXGI_FORMAT kNormalRoughnessFormat    = DXGI_FORMAT_R8G8B8A8_UNORM;
            const DXGI_FORMAT kBaseColorMetalnessFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            const DXGI_FORMAT kDiffuseFormat            = DXGI_FORMAT_R16G16B16A16_FLOAT;
            const DXGI_FORMAT kGlossyFormat             = DXGI_FORMAT_R16G16B16A16_FLOAT;

            // Create the denoising textures, each with unordered access.
            uvec2& dims             = _outputDimensions;
            _pDenoisingTexDepthView = createTexture(dims, kDepthViewFormat, "DepthView", true);
            _pDenoisingTexNormalRoughness =
                createTexture(dims, kNormalRoughnessFormat, "Normal / Roughness", true);
            _pDenoisingTexBaseColorMetalness =
                createTexture(dims, kBaseColorMetalnessFormat, "Base Color / Metalness", true);
            _pDenoisingTexDiffuse    = createTexture(dims, kDiffuseFormat, "Diffuse Input", true);
            _pDenoisingTexGlossy     = createTexture(dims, kGlossyFormat, "Glossy Input", true);
            _pDenoisingTexDiffuseOut = createTexture(dims, kDiffuseFormat, "Diffuse Output", true);
            _pDenoisingTexGlossyOut  = createTexture(dims, kGlossyFormat, "Glossy Output", true);
        }
    }
    else
    {
        // If there is any existing denoising resource (using one to represent all of them),
        // flush the renderer to make sure the denoising resources are no longer being used by
        // the pipeline, then clear the pointers.
        if (_pDenoisingTexDepthView)
        {
            waitForTask();
            _pDenoisingTexDepthView.Reset();
            _pDenoisingTexNormalRoughness.Reset();
            _pDenoisingTexBaseColorMetalness.Reset();
            _pDenoisingTexDiffuse.Reset();
            _pDenoisingTexGlossy.Reset();
            isTexturesUpdated = true;
        }
    }

    // If textures have been updated, or the descriptor heap has changed, create UAVs for the
    // textures.
    if (isTexturesUpdated || _isDescriptorHeapChanged)
    {
        // Flush the renderer to make sure the (unchanged) descriptor heap is not being used.
        if (!_isDescriptorHeapChanged)
        {
            waitForTask();
        }

        // Create UAVs for the denoising textures as the entries in the descriptor heap *after* the
        // descriptors for the output-related textures.
        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            kOutputDescriptorCount, _handleIncrementSize);
        createUAV(_pDenoisingTexDepthView.Get(), handle);
        createUAV(_pDenoisingTexNormalRoughness.Get(), handle);
        createUAV(_pDenoisingTexBaseColorMetalness.Get(), handle);
        createUAV(_pDenoisingTexDiffuse.Get(), handle);
        createUAV(_pDenoisingTexGlossy.Get(), handle);
        createUAV(_pDenoisingTexDiffuseOut.Get(), handle);
        createUAV(_pDenoisingTexGlossyOut.Get(), handle);
    }

#if defined(ENABLE_DENOISER)
    // Create or destroy the denoiser as needed. This can depend on the denoising resources
    // created above.
    if (_isDenoisingEnabled)
    {
        // Create a new denoiser if needed.
        bool isNewDenoiser = false;
        if (!_pDenoiser)
        {
            _pDenoiser = make_unique<Denoiser>(_pDXDevice.Get(), _pCommandQueue.Get(), _taskCount);
            isNewDenoiser = true;
        }

        // If the denoiser is new, or the dimensions have changed (i.e. new resource),
        // initialize the denoiser.
        if (isNewDenoiser || _isDimensionsChanged)
        {
            // Collect the denoising textures.
            // clang-format off
            Denoiser::Textures textures =
            {
                _pDenoisingTexDepthView.Get(),
                _pDenoisingTexNormalRoughness.Get(),
                _pDenoisingTexDiffuse.Get(),
                _pDenoisingTexGlossy.Get(),
                _pDenoisingTexDiffuseOut.Get(),
                _pDenoisingTexGlossyOut.Get()
            };
            // clang-format on

            // Initialize the denoiser.
            _pDenoiser->initialize(_outputDimensions, textures);
        }
    }
    else if (_pDenoiser)
    {
        // Wait for tasks to complete before destroying the denoiser, as internal resources may
        // still be in use.
        waitForTask();
        _pDenoiser.reset();
    }
#endif // ENABLE_DENOISER
}

void PTRenderer::prepareRayDispatch(D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc)
{
    // Prepare a ray dispatch description, including the dimensions...
    dispatchRaysDesc.Width  = _outputDimensions.x;
    dispatchRaysDesc.Height = _outputDimensions.y;
    dispatchRaysDesc.Depth  = 1;

    // ... the shader table for the ray generation shader...
    D3D12_GPU_VIRTUAL_ADDRESS address = _rayGenShaderTable.pGPUBuffer->GetGPUVirtualAddress();
    dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = address;
    dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes  = _rayGenShaderTableSize;

    // ... the shader table for the miss shader(s)...
    size_t missShaderRecordStride  = 0;
    uint32_t missShaderRecordCount = 0;
    ID3D12ResourcePtr pMissShaderTable =
        dxScene()->getMissShaderTable(missShaderRecordStride, missShaderRecordCount);
    dispatchRaysDesc.MissShaderTable.StartAddress  = pMissShaderTable->GetGPUVirtualAddress();
    dispatchRaysDesc.MissShaderTable.StrideInBytes = missShaderRecordStride;
    dispatchRaysDesc.MissShaderTable.SizeInBytes   = missShaderRecordStride * missShaderRecordCount;

    // ... the shader table for the hit group(s).
    // NOTE: If there are no instances, the hit group shader table will be nullptr.
    size_t hitGroupShaderRecordStride  = 0;
    uint32_t hitGroupShaderRecordCount = 0;
    ID3D12ResourcePtr pHitGroupShaderTable =
        dxScene()->getHitGroupShaderTable(hitGroupShaderRecordStride, hitGroupShaderRecordCount);
    dispatchRaysDesc.HitGroupTable.StartAddress =
        pHitGroupShaderTable ? pHitGroupShaderTable->GetGPUVirtualAddress() : 0;
    dispatchRaysDesc.HitGroupTable.StrideInBytes = hitGroupShaderRecordStride;
    dispatchRaysDesc.HitGroupTable.SizeInBytes =
        hitGroupShaderRecordStride * hitGroupShaderRecordCount;
}

void PTRenderer::submitRayDispatch(
    const D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc, uint32_t sampleIndex, uint32_t seedOffset)
{
    // Begin a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = beginCommandList();

    // Prepare the descriptor heaps for CBV/SRV/UAV and for samplers.
    ID3D12DescriptorHeap* pDescriptorHeaps[] = { _pDescriptorHeap.Get(),
        _pSamplerDescriptorHeap.Get() };

    // Prepare the root signature, and pipeline state.
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);
    pCommandList->SetComputeRootSignature(_pShaderLibrary->globalRootSignature().Get());
    pCommandList->SetPipelineState1(_pShaderLibrary->pipelineState().Get());

    // Set the global root signature arguments.
    {
        // 0) The acceleration structure.
        D3D12_GPU_VIRTUAL_ADDRESS accelStructureAddress =
            dxScene()->accelerationStructure()->GetGPUVirtualAddress();
        pCommandList->SetComputeRootShaderResourceView(0, accelStructureAddress);

        // 1) The sample index, as a root constant.
        SampleData sampleData = { sampleIndex, seedOffset };
        pCommandList->SetComputeRoot32BitConstants(1, 2, &sampleData, 0);

        // 2) The frame data constant buffer, for the current task index.
        D3D12_GPU_VIRTUAL_ADDRESS frameDataBufferAddress =
            _frameDataBuffer.pGPUBuffer->GetGPUVirtualAddress() + _FRAME_DATA_SIZE * _taskIndex;
        pCommandList->SetComputeRootConstantBufferView(2, frameDataBufferAddress);

        // 3) The environment data constant buffer.
        ID3D12Resource* pEnvironmentDataBuffer = dxScene()->environment()->buffer();
        D3D12_GPU_VIRTUAL_ADDRESS environmentDataBufferAddress =
            pEnvironmentDataBuffer->GetGPUVirtualAddress();
        pCommandList->SetComputeRootConstantBufferView(3, environmentDataBufferAddress);

        // 4) The environment alias map structured buffer.
        // NOTE: Structured buffers must be specified with an SRV, not a CBV.
        ID3D12Resource* pEnvironmentAliasMapBuffer = dxScene()->environment()->aliasMap();
        D3D12_GPU_VIRTUAL_ADDRESS environmentAliasMapBufferAddress =
            pEnvironmentAliasMapBuffer ? pEnvironmentAliasMapBuffer->GetGPUVirtualAddress() : 0;
        pCommandList->SetComputeRootShaderResourceView(4, environmentAliasMapBufferAddress);

        // 5) The environment texture descriptor table.
        // NOTE: This starts right after the renderer's descriptors, as defined by the scene.
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle(
            _pDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        handle.Offset(kDescriptorCount, _handleIncrementSize);
        pCommandList->SetComputeRootDescriptorTable(5, handle);

        // 6) The ground plane data constant buffer.
        ID3D12Resource* pGroundPlaneDataBuffer = dxScene()->groundPlane()->buffer();
        D3D12_GPU_VIRTUAL_ADDRESS groundPlaneDataBufferAddress =
            pGroundPlaneDataBuffer->GetGPUVirtualAddress();
        pCommandList->SetComputeRootConstantBufferView(6, groundPlaneDataBufferAddress);

        // 7) The null acceleration structure (used for layer material shading).
        D3D12_GPU_VIRTUAL_ADDRESS nullAccelStructAddress = 0;
        pCommandList->SetComputeRootShaderResourceView(7, nullAccelStructAddress);
    }

    // Launch the ray generation shader with the dispatch, which performs path tracing.
    // NOTE: Rendering performance is somewhat choppy if you try to perform multiple dispatches
    // in a single command list. For that reason, there is a separate command list for each path
    // tracing sample (dispatch). That means each command list has mostly redundant setup (above),
    // but that is still a better runtime experience.
    pCommandList->DispatchRays(&dispatchRaysDesc);

    // Submit the command list.
    submitCommandList();
}

#if defined(ENABLE_DENOISER)
void PTRenderer::submitDenoising(bool isRestartRequested)
{
    // Do nothing if denoising is not enabled, i.e. there is no denoiser.
    if (!_pDenoiser)
    {
        return;
    }

    // Begin a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = beginCommandList();

    // Prepare denoiser state.
    Denoiser::DenoiserState denoiserState;
    denoiserState.isRestart         = isRestartRequested;
    denoiserState.taskIndex         = _taskIndex;
    denoiserState.cameraView        = _cameraView;
    denoiserState.cameraProj        = _cameraProj;
    denoiserState.bounds            = dxScene()->bounds();
    denoiserState.pCommandList      = pCommandList.Get();
    denoiserState.pCommandAllocator = getCommandAllocator();

    // Execute the denoiser, which records commands to the command list.
    _pDenoiser->denoise(denoiserState);

    // Submit the command list.
    submitCommandList();
}
#else
void PTRenderer::submitDenoising(bool /*isRestart*/) {}
#endif // ENABLE_DENOISER

void PTRenderer::submitAccumulation(uint32_t sampleIndex)
{
    // Prepare accumulation settings.
    updateAccumulationGPUStruct(sampleIndex);

    // Begin a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = beginCommandList();

    // Add a UAV barrier so that the direct texture can't be used until the previous dispatch is
    // complete.
    addUAVBarrier(_pTexDirect.Get());

    // Prepare the pipeline for accumulation: descriptor heap, root signature, and pipeline
    // state.
    pCommandList->SetDescriptorHeaps(1, _pDescriptorHeap.GetAddressOf());
    pCommandList->SetComputeRootSignature(_pAccumulationRootSignature.Get());
    pCommandList->SetPipelineState(_pAccumulationPipelineState.Get());

    // Set the root signature arguments for accumulation: the descriptor table and the accumulation
    // settings. The descriptor table must start with the accumulation texture.
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        kAccumulationDescriptorOffset, _handleIncrementSize);
    pCommandList->SetComputeRootDescriptorTable(0, handle);
    pCommandList->SetComputeRoot32BitConstants(1, sizeof(Accumulation) / 4, &_accumData, 0);

    // Dispatch the accumulation shader, which performs (optional) deferred shading and merges new
    // path tracing samples with the previous results.
    // NOTE: The dispatch is performed with thread group dimensions that provide good occupancy for
    // the current compute shader code. This must match the values in the compute shader.
    constexpr uvec2 kThreadGroupCount(16, 8);
    pCommandList->Dispatch((_outputDimensions.x + kThreadGroupCount.x - 1) / kThreadGroupCount.x,
        (_outputDimensions.y + kThreadGroupCount.y - 1) / kThreadGroupCount.y, 1);

    // Submit the command list.
    submitCommandList();
}

void PTRenderer::submitPostProcessing()
{
    // Update the post processing GPU struct by calling base class function.
    updatePostProcessingGPUStruct();

    // Begin a command list.
    ID3D12GraphicsCommandList4Ptr pCommandList = beginCommandList();

    // Add a UAV barrier so that the accumulation texture can't be used until the previous dispatch
    // is complete.
    addUAVBarrier(_pTexAccumulation.Get());

    // Prepare the pipeline for post-processing: descriptor heap, root signature, and pipeline
    // state.
    pCommandList->SetDescriptorHeaps(1, _pDescriptorHeap.GetAddressOf());
    pCommandList->SetComputeRootSignature(_pPostProcessingRootSignature.Get());
    pCommandList->SetPipelineState(_pPostProcessingPipelineState.Get());

    // Set the root signature arguments for post-processing: the descriptor table (at the start of
    // the heap) and the post-processing settings. The descriptor table must start with the final
    // texture.
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        kFinalDescriptorOffset, _handleIncrementSize);
    pCommandList->SetComputeRootDescriptorTable(0, handle);
    pCommandList->SetComputeRoot32BitConstants(
        1, sizeof(PostProcessing) / 4, &_postProcessingData, 0);

    // Dispatch the post-processing shader, which tone maps(as needed) the accumulation texture
    // (HDR) to the final texture (usually SDR).
    // NOTE: This should be done even if the settings mean no post-processing is performed as there
    // is still an implicit format conversion, from the accumulation texture (UAV) format to the
    // output texture format, e.g. floating-point to integer.
    // NOTE: The dispatch is performed with thread group dimensions that provide good occupancy for
    // the current compute shader code. This must match the values in the compute shader.
    constexpr uvec2 kThreadGroupCount(16, 8);
    pCommandList->Dispatch((_outputDimensions.x + kThreadGroupCount.x - 1) / kThreadGroupCount.x,
        (_outputDimensions.y + kThreadGroupCount.y - 1) / kThreadGroupCount.y, 1);

    // Copy the output textures to their associated targets, if any.
    copyTextureToTarget(_pTexFinal.Get(), _pTargetFinal.get());
    if (_pTargetDepthNDC)
    {
        copyTextureToTarget(_pTexDepthNDC.Get(), _pTargetDepthNDC.get());
    }

    // Submit the command list.
    submitCommandList();
}

void PTRenderer::createUAV(ID3D12Resource* pTexture, CD3DX12_CPU_DESCRIPTOR_HANDLE& handle)
{
    // Create two static descriptions: one for UAVs when the resource is present, and another
    // for when the resource is not present. The latter is used to create a null descriptor; in that
    // case, the UAV description must have an arbitrary format.
    static D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc     = {};
    uavDesc.ViewDimension                               = D3D12_UAV_DIMENSION_TEXTURE2D;
    static D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescNull = {};
    uavDescNull.Format                                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDescNull.ViewDimension                           = D3D12_UAV_DIMENSION_TEXTURE2D;

    // Create a valid or null descriptor for the texture, depending on whether the texture is
    // null. Then offset the handle by the increment, for creating a subsequent descriptor.
    D3D12_UNORDERED_ACCESS_VIEW_DESC* pUAVDesc = pTexture ? &uavDesc : &uavDescNull;
    _pDXDevice->CreateUnorderedAccessView(pTexture, nullptr, pUAVDesc, handle);
    handle.Offset(_handleIncrementSize);
}

void PTRenderer::copyTextureToTarget(ID3D12Resource* pTexture, PTTarget* pTarget)
{
    // Transition the final texture from writing (rendering to copying), then copy the final
    // texture to the target, and then transition back.
    addTransitionBarrier(
        pTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    pTarget->copyFromResource(pTexture, _pCommandList.Get());
    addTransitionBarrier(
        pTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

bool PTRenderer::isDenoisingAOVsEnabled() const
{
    // Denoising AOVs are enabled when either denoising is enabled, or the debug mode involves
    // rendering with a denoising AOV.
    return _values.asBoolean(kLabelIsDenoisingEnabled) ||
        _values.asInt(kLabelDebugMode) > kDebugModeErrors;
}

shared_ptr<MaterialShader> PTRenderer::generateMaterialX([[maybe_unused]] const string& document,
    [[maybe_unused]] shared_ptr<MaterialDefinition>* pDefOut)
{
#if ENABLE_MATERIALX
    // Generate the material definition for the materialX document, this contains the source code,
    // default values, and a unique name.
    shared_ptr<MaterialDefinition> pDef = _pMaterialXGenerator->generate(document);
    if (!pDef)
    {
        return nullptr;
    }

    // Acquire a material shader for the definition.
    // This will create a new one if needed (and trigger a rebuild), otherwise will it will return
    // existing one.
    auto pShader = _pShaderLibrary->acquireShader(*pDef);

    // Output the definition pointer.
    if (pDefOut)
        *pDefOut = pDef;

    return pShader;
#else
    return nullptr;
#endif
}

END_AURORA
