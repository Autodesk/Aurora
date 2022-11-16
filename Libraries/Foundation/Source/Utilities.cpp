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

#include "Aurora/Foundation/Utilities.h"
#include "Aurora/Foundation/Log.h"

// For UNIX equivalent of GetModuleFileName to get the location of shared library
#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#endif

namespace Aurora
{
namespace Foundation
{

void hashCombine(size_t& seed, size_t otherHash)
{
    seed ^= otherHash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t hashInts(const uint32_t* pFirst, size_t count)
{
    size_t seed = std::hash<size_t>()(count);
    for (size_t i = 0; i < count; i++)
    {
        hashCombine(seed, std::hash<uint32_t>()(pFirst[i]));
    }
    return seed;
}

std::string replace(
    const std::string& str, const std::string& searchTerm, const std::string& replaceTerm)
{
    std::string res = str;
    for (size_t index = res.find(searchTerm, 0); index != std::string::npos && searchTerm.length();
         index        = res.find(searchTerm, index + replaceTerm.length()))
        res.replace(index, searchTerm.length(), replaceTerm);
    return res;
}

std::string getModulePath()
{
#if defined(WIN32)
    char path[MAX_PATH + 1];

    DWORD pathSize = _MAX_PATH;
    if (path == nullptr)
        return std::string();

    // Get a handle to the module that this static function lives in.
    HMODULE hModule = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)getModulePath, &hModule);

    DWORD size = GetModuleFileNameA(hModule, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        AU_FAIL("Failed to get module path.");
    }
    std::string tempBuf(path);

#else
    Dl_info info;
    std::string tempBuf("/");
    if (dladdr((void*)getModulePath, &info))
    {
        tempBuf += info.dli_fname;
    }
    else
    {
        AU_FAIL("Failed to get module path.");
    }
#endif

    size_t charOffset = tempBuf.find('/');
    while (charOffset != std::string::npos)
    {
        tempBuf.replace(charOffset, 1, "\\");
        charOffset = tempBuf.find('/');
    }

    // runTimeDir contains path up to executable name
    // Remove the executable name.
    charOffset = tempBuf.rfind(L'\\');
    if (charOffset != std::string::npos)
        tempBuf.erase(charOffset + 1, (tempBuf.length() - charOffset) - 1);
    return tempBuf;
}

} // namespace Foundation
} // namespace Aurora
