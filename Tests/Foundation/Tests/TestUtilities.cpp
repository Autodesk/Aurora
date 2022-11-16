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

#ifndef DISABLE_UNIT_TESTS

#include <Aurora/Foundation/Utilities.h>
#include <gtest/gtest.h>

#include "TestHelpers.h"

using namespace std;
using namespace Aurora::Foundation;

namespace
{

class UtilitiesTest : public ::testing::Test
{
public:
    UtilitiesTest() {}
    ~UtilitiesTest() {}

    // Optionally override the base class's setUp function to create some
    // data for the tests to use.
    virtual void SetUp() override {}

    // Optionally override the base class's tearDown to clear out any changes
    // and release resources.
    virtual void TearDown() override {}
};

TEST_F(UtilitiesTest, TestUtilities)
{
    std::string utf8str = w2s(L"Fooo");
    ASSERT_EQ(utf8str.compare("Fooo"), 0);

    std::wstring utf16str = s2w("Bar");
    ASSERT_EQ(utf16str.compare(L"Bar"), 0);

    std::string caseStr = "FOOBar";
    sLower(caseStr);
    ASSERT_EQ(caseStr.compare("foobar"), 0);

    std::string fmtStr = sFormat("Thingy:%s stuff:%d", "bar", 42);
    ASSERT_EQ(fmtStr.compare("Thingy:bar stuff:42"), 0);

    std::string hashStr = sHash(0xFEEDCAFE);
    ASSERT_EQ(hashStr.compare("feedcafe"), 0);

    std::string modulePath = getModulePath();
    ASSERT_NE(modulePath.size(), 0);
}

} // namespace

#endif
