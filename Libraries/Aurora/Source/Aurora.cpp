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

#if DIRECTX_SUPPORT
#include "DirectX/PTRenderer.h"
#endif
#if HGI_SUPPORT
#include "HGI/HGIRenderer.h"
#endif
#include "RendererBase.h"

BEGIN_AURORA

Foundation::Log& logger()
{
    return Foundation::Log::logger();
}

IRendererPtr createRenderer(IRenderer::Backend type, [[maybe_unused]] uint32_t taskCount)
{
    RendererBasePtr pRenderer;

    // Create an instance of the appropriate renderer class, with the specified active task count.
    switch (type)
    {
    case IRenderer::Backend::DirectX:
#if DIRECTX_SUPPORT
        pRenderer = make_shared<PTRenderer>(taskCount);
#else
        AU_FAIL(
            "ENABLE_DIRECTX_BACKEND option must be enabled in CMake to support DirectX back end.",
            type);
#endif
        break;
    case IRenderer::Backend::HGI:
#if HGI_SUPPORT
        pRenderer = make_shared<HGIRenderer>(taskCount);
#else
        AU_FAIL(
            "ENABLE_HGI_BACKEND option must be enabled in CMake to support HGI back end.", type);
#endif
        break;
    default:
#if DIRECTX_SUPPORT
        pRenderer = make_shared<PTRenderer>(taskCount);
#elif HGI_SUPPORT
        pRenderer = make_shared<HGIRenderer>(taskCount);
#else
        AU_FAIL("No backend available.");
#endif
        break;
    }
    AU_ASSERT(pRenderer, "Invalid renderer type %x", type);

    // Return nullptr if the renderer did not initialize; otherwise return it.
    return pRenderer->isValid() ? pRenderer : nullptr;
}

END_AURORA