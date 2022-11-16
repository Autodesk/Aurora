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

#include "TestHelpers.h"

// Platform-specific implementation for GETCWD
#if WIN32
#include <direct.h>
#define _GETCWD _getcwd
#else
#include <unistd.h>
#define _GETCWD getcwd
#endif

#include <algorithm>
#include <sys/stat.h>

// Compatibility macros for stat file access modes
#ifndef _WIN32
#define S_IWRITE S_IWUSR
#define S_IREAD S_IRUSR
#endif

namespace TestHelpers
{

// Helper function to convert float32 to/from float16(IEEE 754)
uint32_t as_uint(const float x)
{
    return *(uint32_t*)&x;
}
float as_float(const uint32_t x)
{
    return *(float*)&x;
}

float halfToFloat(const uint16_t x)
{ // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0,
  // +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t e = (x & 0x7C00) >> 10; // exponent
    const uint32_t m = (x & 0x03FF) << 13; // mantissa
    const uint32_t v =
        as_uint((float)m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) |
        ((e == 0) & (m != 0)) *
            ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
}

uint16_t floatToHalf(const float x)
{ // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0,
  // +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t b =
        as_uint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint32_t e = (b & 0x7F800000) >> 23; // exponent
    const uint32_t m =
        b & 0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 =
                        // decimal indicator flag - initial rounding
    return static_cast<uint16_t>((b & 0x80000000) >> 16 |
        (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) |
        ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) |
        (e > 143) * 0x7FFF); // sign : normalized : denormalized : saturate
}

float createFloat32(uint32_t mantisa, uint32_t exponent, uint32_t sign)
{
    DecompFloat32 f;
    f.parts.mantisa  = mantisa;
    f.parts.exponent = exponent;
    f.parts.sign     = sign;

    return f.f;
}

uint16_t createFloat16(uint16_t mantisa, uint16_t exponent, uint16_t sign)
{
    DecompFloat16 f;
    f.parts.mantisa  = mantisa;
    f.parts.exponent = exponent;
    f.parts.sign     = sign;

    return f.f;
}

std::string getEnvironmentVariable(const std::string& name)
{
#ifdef _WIN32
    // On windows use _dupenv_s to get environment variable to buffer.
    char* buffer;
    size_t bufferSize;
    if (_dupenv_s(&buffer, &bufferSize, name.c_str()) != 0)
    {
        return "";
    }

    // If null return empty string.
    if (!buffer)
    {
        return "";
    }
#else
    // Other platforms use getenv
    char* buffer = getenv(name.c_str());

    // If null return empty string.
    if (!buffer)
    {
        return "";
    }
#endif

    return buffer;
}

bool getIntegerEnvironmentVariable(const std::string& name, int* valueOut)
{
    // Get env. var. as string.
    std::string envVar = getEnvironmentVariable(name);

    // If empty string return false.
    if (envVar.size() == 0)
    {
        return false;
    }

    // Convert value to int.
    if (valueOut)
    {
        *valueOut = std::atoi(envVar.c_str());
    }

    return true;
}

bool getFlagEnvironmentVariable(const std::string& name)
{
    int val;
    if (!getIntegerEnvironmentVariable(name, &val))
        return false;

    return val != 0;
}

std::string combinePaths(const std::string& path0, const std::string& path1)
{
    // Just combine with / for now
    // TODO: Better platform-specific solution, handle edge cases
    return path0 + "/" + path1;
}

void sanitizeFileName(std::string& fileName)
{
    // Replace non-file chars with underscore
    // TODO: More exhaustive set of chars
    std::replace(fileName.begin(), fileName.end(), '/', '_');
    std::replace(fileName.begin(), fileName.end(), '\\', '_');
    std::replace(fileName.begin(), fileName.end(), '?', '_');
    std::replace(fileName.begin(), fileName.end(), '*', '_');
}

bool createDirectory(const std::string& path)
{
    // If already exists return true
    if (directoryExists(path))
        return true;

    // integer result code
    int res;

#if _WIN32
    // On Windows use _mkdir
    res = _mkdir(path.c_str());
#else
    // On other platforms use mkdir (with default access mode)
    mode_t mode = 0733;
    res         = mkdir(path.c_str(), mode);
#endif

    // Non-zero result is failure
    return res == 0;
}

bool directoryExists(const std::string& path)
{
    // Struct for stat results
    struct stat info;

    // Use stat to get file info (return false if result non-zero to indicate failure)
    if (stat(path.c_str(), &info) != 0)
        return false;

    // Return true if directory flag is set
    return (info.st_mode & S_IFDIR);
}

bool fileIsWritable(const std::string& path)
{
    // Struct for stat results
    struct stat info;

    // Use stat to get file info (return false if result non-zero to indicate failure)
    if (stat(path.c_str(), &info) != 0)
        return false;

    // If its a directory not file, return false
    if (info.st_mode & S_IFDIR)
        return false;

    // Return true if write flag is set
    return (info.st_mode & S_IWRITE) != 0;
}

bool fileIsReadable(const std::string& path)
{
    // Struct for stat results
    struct stat info;

    // Use stat to get file info (return false if result non-zero to indicate failure)
    if (stat(path.c_str(), &info) != 0)
        return false;

    // If its a directory not file, return false
    if (info.st_mode & S_IFDIR)
        return false;

    // Return true if read flag is set
    return (info.st_mode & S_IREAD) != 0;
}

} // namespace TestHelpers
