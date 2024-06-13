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

#undef snprintf

#include <algorithm>
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

namespace Aurora
{
namespace Foundation
{

/// Remove illegal chars from a filename.
void sanitizeFileName(std::string& fileName);

/// Write string to file, returns false if write fails.
bool writeStringToFile(
    const std::string& str, const std::string& filename, const std::string& folder = "");

/// Combine a hash with a seed value.
void hashCombine(size_t& seed, size_t otherHash);

/// Compute a hash from an array of ints.
std::size_t hashInts(const uint32_t* pFirst, size_t count);

/// Converts a UTF-16 string to UTF-8 (variable-width encoding).
///
/// \note This will throw a range_error exception if the input is invalid.
inline std::string w2s(const std::wstring& utf16Source)
{
// TODO: Work out how to avoid deprecated warnings, as there is nothing to replace this in the
// standard.
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    return converter.to_bytes(utf16Source);
#pragma clang diagnostic pop
#pragma warning(pop)
}

/// Converts a UTF-8 string to UTF-16 (variable-width encoding).
///
/// \note This will throw a range_error exception if the input is invalid.
inline std::wstring s2w(const std::string& utf8Source)
{
// TODO: Work out how to avoid deprecated warnings, as there is nothing to replace this in the
// standard.
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    return converter.from_bytes(utf8Source);
#pragma clang diagnostic pop
#pragma warning(pop)
}

/// Converts the provided string to lowercase, in place.
inline void sLower(std::string& data)
{
    std::transform(data.begin(), data.end(), data.begin(),
        [](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
}

// Convert 64-bit hash to hex string.
inline const std::string sHash(size_t hash)
{
    // Create a string stream and ensure locale does not add extra characters.
    std::stringstream sstream;
    sstream.imbue(std::locale::classic());

    // Convert to hex string.
    // See unit test TestUtilities in Tests\Foundation\API\TestUtilities.cpp for example.
    sstream << std::setfill('0') << std::setw(2) << std::hex << hash;
    return sstream.str();
}

/// Creates a string from variable arguments.
template <typename... Args>
std::string sFormat(const std::string& format, Args... args)
{
    // Disable the Clang security warning.
    // TODO: Implement a better workaround for this warning.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
    // Calculate the size of the expanded format string.
    const char* formatStr = format.c_str();
    int size = std::snprintf(nullptr, 0, formatStr, args...) + 1; // extra space for the \0
    if (size <= 0)
    {
        // Report an error if formating fails.
        std::cerr << __FUNCTION__ << " Error during formatting." << std::endl;

        return "";
    }

    // Allocate a buffer for the formatted string and place the expanded string in it.
    std::vector<char> buf(size);
    std::snprintf(buf.data(), size, formatStr, args...);
#if __clang__
#pragma clang diagnostic pop
#endif

    // Return a new string object from the buffer.
    // TODO: Avoid multiple allocations.
    return std::string(buf.data(), buf.data() + size - 1); // don't include the \0
};

/// Get the absolute path for the currently executing module.
std::string getModulePath();

/// Wrap a positive or negative integer x in the range 0 to y-1.
inline int iwrap(int x, int y)
{
    // If x is positive return x mod y.
    if (x > 0)
        return x % y;

    // If x is negative compute modulo, then add y-1.
    if (x < 0)
        return (x + 1) % y + y - 1;

    return 0;
}

/// Return a string where all occurences of searchTerm in src are replaced with replaceTerm.
std::string replace(
    const std::string& str, const std::string& searchTerm, const std::string& replaceTerm);

} // namespace Foundation
} // namespace Aurora
