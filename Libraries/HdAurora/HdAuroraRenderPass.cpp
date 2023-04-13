#include "pch.h"

#include "HdAuroraRenderBuffer.h"
#include "HdAuroraRenderDelegate.h"
#include "HdAuroraRenderPass.h"
#include "HdAuroraTokens.h"
#include <pxr/imaging/garch/glApi.h>

HdAuroraRenderPass::HdAuroraRenderPass(HdRenderIndex* index, HdRprimCollection const& collection,
    HdAuroraRenderDelegate* renderDelegate) :
    HdRenderPass(index, collection),
    _owner(renderDelegate),
    _frameBufferTexture(0),
    _frameBufferObject(0),
    _ownedRenderBuffer(nullptr)
{
    _renderBuffers[HdAovTokens->color] = nullptr;
}

HdAuroraRenderPass::~HdAuroraRenderPass()
{
    _owner->RenderPassDestroyed(this);
}

bool HdAuroraRenderPass::IsConverged() const
{
    return _owner->GetSampleCounter().isComplete();
}

void HdAuroraRenderPass::_Execute(
    HdRenderPassStateSharedPtr const& renderPassState, TfTokenVector const& /* renderTags */)
{
    // GetViewport has been deprecated. Only use when camera framing is not valid.
    const CameraUtilFraming& framing = renderPassState->GetFraming();
    int viewportWidth                = framing.IsValid() ? framing.dataWindow.GetWidth()
                                                         : static_cast<int>(renderPassState->GetViewport()[2]);
    int viewportHeight               = framing.IsValid() ? framing.dataWindow.GetHeight()
                                                         : static_cast<int>(renderPassState->GetViewport()[3]);

    HdFormat format = _owner->GetDefaultAovDescriptor(HdAovTokens->color).format;

    if (_viewportSize.first != viewportWidth || _viewportSize.second != viewportHeight)
    {
        _renderBuffers[HdAovTokens->color] = nullptr;
    }

    // Set up AOV bindings based on bindings in renderPassState passed in from Hydra
    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    if (aovBindings.size() == 0)
    {
        format = HdFormatUNorm8Vec4;

        // If AOV Bindings in the Hydra render pass state are empty
        // it is up to the plugin to own the frame buffer
        if (!_ownedRenderBuffer)
        {
            // Create a render buffer to render into
            _ownedRenderBuffer =
                std::make_shared<HdAuroraRenderBuffer>(SdfPath::EmptyPath(), _owner);

            // create openGL frame buffer texture
            char imageData[4 * 4 * 4];
            glGenTextures(1, &_frameBufferTexture);
            glBindTexture(GL_TEXTURE_2D, _frameBufferTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

            // create openGL frame buffer object
            glGenFramebuffers(1, &_frameBufferObject);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, _frameBufferObject);
            glFramebufferTexture2D(
                GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _frameBufferTexture, 0);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);

            // Create a set of AOV bindings from our render buffer
            HdRenderPassAovBinding colorAov;
            colorAov.aovName      = HdAovTokens->color;
            colorAov.renderBuffer = _ownedRenderBuffer.get();
            // Set the default clear color (this is currently ignored we should set our clear color
            // based on value in AOV bindings)
            colorAov.clearValue = VtValue(GfVec4f(0.05f, 0.05f, 0.09f, 1.0f));
            _aovBindings.clear();
            _aovBindings.push_back(colorAov);
        }
        _renderBuffers[HdAovTokens->color] = _ownedRenderBuffer.get();
    }
    else if (!_renderBuffers[HdAovTokens->color] || _aovBindings != aovBindings)
    {
        // If we are given AOV bindings we should use them (currently we only care about color
        // AOV)

        // Clear the current buffers
        _renderBuffers[HdAovTokens->color] = nullptr;
        _ownedRenderBuffer.reset();

        // Find the color buffer in the AOV bindings
        for (size_t i = 0; i < aovBindings.size(); i++)
        {
            if (aovBindings[i].aovName == HdAovTokens->color)
            {
                // Color buffers are requested as 32bit float buffers when using AOVs
                _renderBuffers[HdAovTokens->color] =
                    static_cast<HdAuroraRenderBuffer*>(aovBindings[i].renderBuffer);
                format = HdFormatFloat16Vec4;
            }
            else if (aovBindings[i].aovName == HdAovTokens->depth)
            {
                _renderBuffers[HdAovTokens->depth] =
                    static_cast<HdAuroraRenderBuffer*>(aovBindings[i].renderBuffer);
            }
        }
        if (!_renderBuffers[HdAovTokens->color])
            std::cerr << "Failed to find color buffer in AOV bindings" << std::endl;
        _aovBindings = aovBindings;
    }

    // resize Aurora render buffer
    if (!_renderBuffers[HdAovTokens->color]->IsAllocated() ||
        _viewportSize.first != viewportWidth || _viewportSize.second != viewportHeight)
    {
        _renderBuffers[HdAovTokens->color]->Allocate(
            GfVec3i(viewportWidth, viewportHeight, 1), format, false);

        auto depthAovIt = _renderBuffers.find(HdAovTokens->depth);
        // do we have a valid depth AOV?
        if (depthAovIt != _renderBuffers.end() && depthAovIt->second != nullptr)
        {
            _renderBuffers[HdAovTokens->depth]->Allocate(GfVec3i(viewportWidth, viewportHeight, 1),
                _owner->GetDefaultAovDescriptor(HdAovTokens->depth).format, false);
        }

        _owner->ActivateRenderPass(this, _renderBuffers);

        _viewportSize.first  = viewportWidth;
        _viewportSize.second = viewportHeight;
        _owner->SetSampleRestartNeeded(true);
    }

    // Get the view and projection matrices using the active camera of the render pass.
    const auto camera = renderPassState->GetCamera();
    GfMatrix4f viewMat(camera->GetTransform().GetInverse());
    GfMatrix4f projMat(camera->ComputeProjectionMatrix());

    // Check to see if we need to flip the output image.
    // Default to true if not set.
    bool flipY      = true;
    auto flipYValue = this->_owner->GetRenderSetting(HdAuroraTokens::kFlipYOutput);
    if (!flipYValue.IsEmpty() && flipYValue.IsHolding<bool>())
        flipY = flipYValue.Get<bool>();
    if (flipY)
    {
        // Invert the second row of the projection matrix to flip the rendered image, because OpenGL
        // expects the first pixel to be at the *bottom* corner.
        projMat[1][0] = -projMat[1][0];
        projMat[1][1] = -projMat[1][1];
        projMat[1][2] = -projMat[1][2];
        projMat[1][3] = -projMat[1][3];
    }

    // Set the view and projection matrices on the renderer.
    _owner->GetRenderer()->setCamera((float*)&viewMat, (float*)&projMat);

    // check for camera movements
    if (_cameraView != viewMat || _cameraProj != projMat)
    {
        _cameraView = viewMat;
        _cameraProj = projMat;
        _owner->SetSampleRestartNeeded(true);
    }

    // Update the background.
    _owner->UpdateAuroraEnvironment();

    // Render the scene.
    uint32_t sampleStart = 0;
    uint32_t sampleCount =
        _owner->GetSampleCounter().update(sampleStart, _owner->SampleRestartNeeded());
    if (sampleCount > 0)
    {
        _owner->GetRenderer()->render(sampleStart, sampleCount);
    }

    // Clear the restart flag.
    _owner->SetSampleRestartNeeded(false);

    // Use HdAuroraRenderPass::IsConverged() here as it supports Rasterization and Path Tracing
    _renderBuffers[HdAovTokens->color]->SetConverged(IsConverged());

    // If render pass owns the render buffer, we are responsible for doing the BLIT to the GL FBO
    if (_ownedRenderBuffer)
    {
        // copy data to openGL texture
        const void* pData = _renderBuffers[HdAovTokens->color]->Map();
        size_t stride     = _renderBuffers[HdAovTokens->color]->GetStride();
        glBindTexture(GL_TEXTURE_2D, _frameBufferTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)stride / 4, viewportHeight, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, pData);
        _renderBuffers[HdAovTokens->color]->Unmap();

        // copy frame buffer texture to the screen
        glBindFramebuffer(GL_READ_FRAMEBUFFER, _frameBufferObject);
        glBlitFramebuffer(0, 0, _viewportSize.first, _viewportSize.second, 0, 0,
            _viewportSize.first, _viewportSize.second, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Restore GL state
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
