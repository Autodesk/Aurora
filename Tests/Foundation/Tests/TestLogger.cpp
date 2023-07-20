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

#ifndef DISABLE_UNIT_TESTS

#include <Aurora/Foundation/Log.h>
#include <gtest/gtest.h>

#include "TestHelpers.h"

using namespace std;
using namespace Aurora::Foundation;

namespace
{

class LoggerTest : public ::testing::Test
{
public:
    LoggerTest() {}
    ~LoggerTest() {}

    // Optionally override the base class's setUp function to create some
    // data for the tests to use.
    virtual void SetUp() override {}

    // Optionally override the base class's tearDown to clear out any changes
    // and release resources.
    virtual void TearDown() override {}
};

} // namespace

struct LogRecord
{
    std::string file;
    int line;
    Log::Level level;
    std::string msg;
};

TEST_F(LoggerTest, TestLogger)
{
    // Setup a callback function to record logs and number of failures.
    std::vector<LogRecord> output;
    std::vector<LogRecord>* pOutput = &output;
    int failureCount                = 0;
    int* pfailureCount              = &failureCount;
    auto cb                         = [pOutput, pfailureCount](
                  const std::string& file, int line, Log::Level level, const std::string& msg) {
        // Push the log onto output vector.
        LogRecord record = {
            file,
            line,
            level,
            msg,
        };
        pOutput->push_back(record);

        // If its a failure, increment the failure count and return false so abort is not triggered.
        if (level == Log::Level::kFail)
        {
            (*pfailureCount)++;
            return false;
        }
        return true;
    };

    // Set the callback function and disable error message boxes;
    Log::logger().setLogFunction(cb);
    Log::logger().enableFailureDialog(false);

    // Print some logs
    int startLine = __LINE__;
    AU_INFO("Foo");
    AU_WARN("Bar:%d", 4);
    AU_ERROR("Oof:%0.3f\n", 4.5f);

    // Ensure correct output
    ASSERT_EQ(output.size(), 3ull);
    ASSERT_STREQ(output[0].msg.c_str(), "Foo\n");
    ASSERT_STREQ(output[1].msg.c_str(), "Bar:4\n");
    ASSERT_STREQ(output[2].msg.c_str(), "Oof:4.500\n");
    ASSERT_EQ(output[0].level, Log::Level::kInfo);
    ASSERT_EQ(output[1].level, Log::Level::kWarn);
    ASSERT_EQ(output[2].level, Log::Level::kError);
    ASSERT_EQ(output[0].line, startLine + 1);
    ASSERT_EQ(output[1].line, startLine + 2);
    ASSERT_EQ(output[2].line, startLine + 3);
    ASSERT_STREQ(output[0].file.c_str(), __FILE__);
    ASSERT_STREQ(output[1].file.c_str(), __FILE__);
    ASSERT_STREQ(output[2].file.c_str(), __FILE__);

    // Disable all logs with level less than warning.
    output.clear();
    Log::logger().setLogLevel(Log::Level::kWarn);
    // Ensure trace not printed.
    AU_INFO("A");
    AU_WARN("B");
    AU_ERROR("C");
    ASSERT_EQ(output.size(), 2ull);

    // Disable all logs with level less than error.
    output.clear();
    Log::logger().setLogLevel(Log::Level::kError);
    // Ensure trace and warning not printed.
    AU_INFO("A");
    AU_WARN("B");
    AU_ERROR("C");
    ASSERT_EQ(output.size(), 1ull);

    // Disable all logs
    output.clear();
    Log::logger().setLogLevel(Log::Level::kNone);
    // Ensure nothing printed.
    AU_INFO("A");
    AU_WARN("B");
    AU_ERROR("C");
    ASSERT_EQ(output.size(), 0ull);

    // Return to default log level.
    Log::logger().setLogLevel(Log::Level::kInfo);

    output.clear();
    failureCount = 0;
    // Trigger a failure with AU_ASSERT
    bool notTrue = false;
    int a        = 42;
    AU_ASSERT(notTrue, "Foo blew up!");
    AU_ASSERT(notTrue, "Foo blew up cos:%s", "thingy");
    AU_ASSERT(notTrue, "Foo blew up cos:%s and %d", "thingy", a);
    ASSERT_EQ(output.size(), 3ull);
    ASSERT_STREQ(output[0].msg.c_str(),
        "AU_ASSERT test failed:\nEXPRESSION: notTrue\nDETAILS: Foo blew up!\n");
    ASSERT_STREQ(output[1].msg.c_str(),
        "AU_ASSERT test failed:\nEXPRESSION: notTrue\nDETAILS: Foo blew up cos:thingy\n");
    ASSERT_STREQ(output[2].msg.c_str(),
        "AU_ASSERT test failed:\nEXPRESSION: notTrue\nDETAILS: Foo blew up cos:thingy and 42\n");
    ASSERT_EQ(output[0].level, Log::Level::kFail);
    ASSERT_EQ(output[1].level, Log::Level::kFail);
    ASSERT_EQ(output[2].level, Log::Level::kFail);
    ASSERT_EQ(failureCount, 3);

    // Trigger a debug-only failure with AU_ASSERT_DEBUG
    // Should only be used where check itself is very expensive.
    [[maybe_unused]] auto veryExpensive = []() { return false; };
    AU_ASSERT_DEBUG(veryExpensive(), "Foo blew up!");
    AU_ASSERT_DEBUG(veryExpensive(), "Foo blew up cos:%s", "thingy");
    AU_ASSERT_DEBUG(veryExpensive(), "Foo blew up cos:%s and %d", "thingy", a);
    if (RUNNING_DEBUG_BUILD)
    {
        ASSERT_EQ(output.size(), 6ull);
        ASSERT_STREQ(output[3].msg.c_str(),
            "AU_ASSERT test failed:\nEXPRESSION: veryExpensive()\nDETAILS: Foo blew up!\n");
        ASSERT_STREQ(output[4].msg.c_str(),
            "AU_ASSERT test failed:\nEXPRESSION: veryExpensive()\nDETAILS: Foo blew up "
            "cos:thingy\n");
        ASSERT_STREQ(output[5].msg.c_str(),
            "AU_ASSERT test failed:\nEXPRESSION: veryExpensive()\nDETAILS: Foo blew up "
            "cos:thingy and 42\n");
        ASSERT_EQ(output[1].level, Log::Level::kFail);
        ASSERT_EQ(failureCount, 6);
    }
    else
    {
        ASSERT_EQ(output.size(), 3ull);
        ASSERT_EQ(failureCount, 3);
    }
}

#endif
