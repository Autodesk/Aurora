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

// STL headers.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <codecvt>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// GLM - OpenGL Mathematics.
// NOTE: This is a math library, and not specific to OpenGL.
#define GLM_FORCE_CTOR_INIT
#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)
using namespace glm;

// Aurora headers.
#include <Aurora/Foundation/BoundingBox.h>
#include <Aurora/Foundation/Log.h>
#include <Aurora/Foundation/Timer.h>
#include <Aurora/Foundation/Utilities.h>
using namespace Aurora;

// Define the module export symbol, before including local declarations.
#if defined(WIN32)
#define AURORA_API __declspec(dllexport)
#else
#define AURORA_API __attribute__((visibility("default")))
#endif

#if !defined(WIN32)
using UINT = unsigned int;
#define CONST const
#define MAX_PATH 2048
#endif

// Namespace symbols.
#define BEGIN_AURORA                                                                               \
    namespace Aurora                                                                               \
    {
#define END_AURORA }

// Utility macros.
// NOTE: In general STL smart pointers and arrays / vectors should be used instead of raw pointers
// and C arrays.
#define SAFE_DELETE(p)                                                                             \
    {                                                                                              \
        if ((p) != nullptr)                                                                        \
        {                                                                                          \
            delete (p);                                                                            \
            (p) = nullptr;                                                                         \
        }                                                                                          \
    }
#define SAFE_DELETE_ARRAY(p)                                                                       \
    {                                                                                              \
        if ((p) != nullptr)                                                                        \
        {                                                                                          \
            delete[](p);                                                                           \
            (p) = nullptr;                                                                         \
        }                                                                                          \
    }
#define ALIGNED_SIZE(size, alignment) ((size / alignment + 1) * alignment)
#define ALIGNED_OFFSET(offset, alignment) (alignment - offset % alignment)

// Platform-specific headers.
#if defined(__APPLE__)
// macOS-specific headers.
// (none yet)
#elif defined(_WIN32)
// Windows-specific headers.
#include "WindowsHeaders.h"
#if DIRECTX_SUPPORT
#include "DirectX/DirectXHeaders.h"
#endif
#endif

#if HGI_SUPPORT
#pragma warning(push)
#pragma warning(disable : 4244) // disabling type conversion warnings in USD
#pragma warning(disable : 4305) // disabling type truncation warnings in USD
#pragma warning(disable : 4127) // disabling const comparison warnings in USD
#pragma warning(disable : 4201) // disabling nameless struct warnings in USD
#pragma warning(disable : 4100) // disabling unreferenced parameter warnings in USD

#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hgi/blitCmds.h>
#include <pxr/imaging/hgi/blitCmdsOps.h>
#include <pxr/imaging/hgi/buffer.h>
#include <pxr/imaging/hgi/enums.h>
#include <pxr/imaging/hgi/handle.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/rayTracingPipeline.h>
#include <pxr/imaging/hgi/resourceBindings.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgi/types.h>
#include <pxr/imaging/hgiInterop/hgiInterop.h>
#pragma warning(pop)

#endif

using namespace std;

// Include the entire Aurora library, a single header file.
// NOTE: This is included here for convenience. If the library is split into multiple header files,
// this should be removed and the relevant header files included where they are needed, e.g.
#include <Aurora/Aurora.h>
