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

#include <Aurora/Foundation/Log.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdint.h>
#include <string>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/// Convenience helper functions for internal use in unit tests
namespace TestHelpers
{

// Exception class to throw from Aurora::Foundation::Log logger callback.
struct AuroraLoggerException : public std::exception
{
    // Pass a reason, filename and line number to constructor, used to build error message.
    AuroraLoggerException(const char* reasonStr, const char* fileStr, int lineNo)
    {
        errorMessage = "AU_FAIL called in " + std::string(fileStr) + " on line " +
            std::to_string(lineNo) + " error message: " + std::string(reasonStr);
    }

    // Override what method, used to report error message in std::exception.
    const char* what() const throw() override { return errorMessage.c_str(); }

    std::string errorMessage;
};

class TestBase : public ::testing::Test
{
public:
    TestBase()
    {
        using Aurora::Foundation::Log;

        // Disable message boxes so we they don't break the tests in debug mode.
        Log::logger().enableFailureDialog(false);

        // Add a custom logger callback that throws an exception for critical errors, this allows
        // better testing for error cases. It also stores last message for testing purposes.
        auto cb = [this](
                      const std::string& file, int line, Log::Level level, const std::string& msg) {
            _lastLogMessage = msg;
            if (level == Log::Level::kFail)
            {
                // Throw exception with error message.
                throw AuroraLoggerException(msg.c_str(), file.c_str(), line);
            }
            return true;
        };
        Log::logger().setLogFunction(cb);
    }

    virtual ~TestBase() {}

    const std::string lastLogMessage() { return _lastLogMessage; }

protected:
    std::string _lastLogMessage;
};

/// \brief Root folder of the source repo
/// Stringify-ed version of CMAKE preprocessor define AURORA_ROOT_PATH
const std::string kSourceRoot(TOSTRING(AURORA_ROOT_PATH));

// Set of symbols to allow runtime conditionals based on platform in tests
#if defined _WIN32
#define RUNNING_ON_WINDOWS 1
#define RUNNING_ON_MAC 0
#define RUNNING_ON_LINUX 0
#define RUNNING_ON_ANDROID 0
#define RUNNING_ON_IPHONE 0
#elif defined __APPLE__
#define RUNNING_ON_WINDOWS 0
#define RUNNING_ON_MAC 1
#define RUNNING_ON_LINUX 0
#define RUNNING_ON_ANDROID 0
#if TARGET_OS_IPHONE
#define RUNNING_ON_IPHONE 1
#else
#define RUNNING_ON_IPHONE 0
#endif
#elif defined __linux__
#define RUNNING_ON_WINDOWS 0
#define RUNNING_ON_MAC 0
#define RUNNING_ON_LINUX 1
#define RUNNING_ON_IPHONE 0
#if defined __ANDROID__
#define RUNNING_ON_ANDROID 1
#else
#define RUNNING_ON_ANDROID 0
#endif
#endif

#if defined NDEBUG
#define RUNNING_RELEASE_BUILD 1
#define RUNNING_DEBUG_BUILD 0
#else
#define RUNNING_RELEASE_BUILD 0
#define RUNNING_DEBUG_BUILD 1
#endif

/// Float32 decomposed into mantisa exponent and sign parts
typedef union
{
    uint32_t i;
    float f;
    struct
    {
        uint32_t mantisa : 23;
        uint32_t exponent : 8;
        uint32_t sign : 1;
    } parts;
} DecompFloat32;

/// Float32 decomposed into mantisa exponent and sign parts
typedef union
{
    uint16_t f;
    struct
    {
        uint16_t mantisa : 10;
        uint16_t exponent : 5;
        uint16_t sign : 1;
    } parts;
} DecompFloat16;

/// \brief Convert 16-bit half float to 32-bit full float
float halfToFloat(const uint16_t x);

/// \brief Convert 32-bit full float to 16-bit half float
uint16_t floatToHalf(const float x);

/// \brief Create float from integer mantisa, exponent, and sign components
float createFloat32(uint32_t mantisa, uint32_t exponent, uint32_t sign);

/// \brief Create half from integer mantisa, exponent, and sign components
uint16_t createFloat16(uint16_t mantisa, uint16_t exponent, uint16_t sign);

/// \brief Get the named environment variable.
//  \return The contents of the variable, or empty string if none.
std::string getEnvironmentVariable(const std::string& name);

/// \brief Get the named environment variable as an integer.
/// \param valueOut Pointer to an integer where the output value is placed.
//  \return True if environment variable is set.
bool getIntegerEnvironmentVariable(const std::string& name, int* valueOut = nullptr);

/// \brief Get the named environment variable as 0 or 1 flag.
//  \return True if environment variable is set and has a non-zero integer value.
bool getFlagEnvironmentVariable(const std::string& name);

/// \brief Combine two path strings with separator
std::string combinePaths(const std::string& path0, const std::string& path1);

/// \brief Create directory in file system, if it doesn't already exist
/// \return false if operation failed
bool createDirectory(const std::string& path);

/// \brief Does directory exist in file system?
/// \return false if directory does not exist (or is regular file not directory)
bool directoryExists(const std::string& path);

/// \brief Is file writable in file system?
/// \return false if file does not exist (or is not writable)
bool fileIsWritable(const std::string& path);

/// \brief Is file readable in file system?
/// \return false if file does not exist (or is not readable)
bool fileIsReadable(const std::string& path);

} // namespace TestHelpers
