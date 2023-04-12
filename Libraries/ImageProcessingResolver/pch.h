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

#include <codecvt>
#include <fstream>
#include <iostream>

using namespace std;

#pragma warning(push)
#pragma warning(disable : 4244) // disabling type conversion warnings in USD
#pragma warning(disable : 4305) // disabling type truncation warnings in USD
#pragma warning(disable : 4127) // disabling const comparison warnings in USD
#pragma warning(disable : 4201) // disabling nameless struct warnings in USD
#pragma warning(disable : 4100) // disabling unreferenced parameter warnings in USD
#pragma warning(disable : 4275) // disabling non dll-interface class used as base for dll-interface
                                // class in USD

// disable clang warnings about deprecated declarations in USD headers
#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// warning: order of some includes here is important for successful compilation
#include <pxr/base/gf/half.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/basisCurves.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/instancer.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hd/task.h>
#include <pxr/imaging/hd/vtBufferSource.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/imaging/hio/types.h>
#include <pxr/pxr.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>

#if defined(__APPLE__)
#pragma clang diagnostic pop
#endif
#pragma warning(pop)

// GLM - OpenGL Mathematics.
// NOTE: This is a math library, and not specific to OpenGL.
#define GLM_FORCE_CTOR_INIT
#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union
#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#pragma warning(pop)

// Aurora.
#include <Aurora/Foundation/Timer.h>
#include <Aurora/Foundation/Log.h>
#include <Aurora/Foundation/Utilities.h>

PXR_NAMESPACE_USING_DIRECTIVE
