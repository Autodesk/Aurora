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

// Memory leak detection.
#if defined(INTERACTIVE_PLASMA)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <cstdlib>

#if defined(INTERACTIVE_PLASMA)
// Windows headers.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
// NOTE: Must come after Windows.h. This comment ensures clang-format does not reorder.
#include <PathCch.h>
#include <Shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#endif

// STL headers.
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// GLM - OpenGL Mathematics
// NOTE: This is a math library, and not specific to OpenGL.
#define GLM_FORCE_CTOR_INIT
// Enable glm experimental for transform.hpp.
#define GLM_ENABLE_EXPERIMENTAL
#pragma warning(push)
#pragma warning(disable : 4127) // nameless struct/union
#pragma warning(disable : 4201) // conditional expression is not constant
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#pragma warning(pop)
using namespace glm;

// STB libraries.
#pragma warning(push)
#pragma warning(disable : 4996)
#include <stb_image_write.h>
#pragma warning(pop)

// 3D model file loaders.
#include <tiny_gltf.h>
#include <tiny_obj_loader.h>

// cxxopts for command line argument parsing.
#include <cxxopts.hpp>

// Aurora.
#include <Aurora/Aurora.h>
#include <Aurora/Foundation/BoundingBox.h>
#include <Aurora/Foundation/Timer.h>
#include <Aurora/Foundation/Utilities.h>
using namespace Aurora;

// Module resources.
#include "resource.h"

#if defined(INTERACTIVE_PLASMA)
// TODO move those windows implementations to a utility class
// Set the window to use for messages.
extern HWND gWindow;
inline void setMessageWindow(HWND hwnd)
{
    gWindow = hwnd;
}
#endif

// Displays an information message.
inline void infoMessage(const string& message)
{
    AU_INFO(message);
}

// Displays an error message.
inline void errorMessage(const string& message)
{
    AU_ERROR(message);
#if defined(INTERACTIVE_PLASMA)
    ::MessageBoxW(gWindow, Foundation::s2w(message).c_str(), L"Error", MB_OK);
#endif
}

// Linearizes a single sRGB color component and returns it.
inline float sRGBToLinear(float value)
{
    return value * (value * (value * 0.305306011f + 0.682171111f) + 0.012522878f);
}

// Linearizes an sRGB color and returns it.
inline vec3 sRGBToLinear(const vec3& color)
{
    vec3 result;
    result.r = sRGBToLinear(color.r);
    result.g = sRGBToLinear(color.g);
    result.b = sRGBToLinear(color.b);

    return result;
}

// Utility macro to convert preprocessor value to string.
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
