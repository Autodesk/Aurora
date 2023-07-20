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

#if !defined(DISABLE_UNIT_TESTS)

#include "TestHelpers.h"
#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <streambuf>

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

namespace
{

// Test fixture accessing test asset data path.
class PropertiesTest : public ::testing::Test
{
public:
    PropertiesTest() {}
    ~PropertiesTest() {}
};

// Basic properties test.
TEST_F(PropertiesTest, BasicTest)
{
    Aurora::Properties props = {
        { "foo", 1 },
        { "bar", 1.5f },
        { "truthy", true },
        { "v2", glm::vec2(1, 2) },
        { "v3", glm::vec3(1, 2, 3) },
        { "v4", glm::vec4(0.1f, 0.2f, 0.3f, 0.4f) },
        { "mtx", glm::translate(glm::mat4(), glm::vec3(4, 5, 6)) },
        { "blooop", "blaap" },
    };

    ASSERT_EQ(props["foo"].type, Aurora::PropertyValue::Type::Int);
    ASSERT_EQ(props["foo"].asInt(), 1);
    ASSERT_EQ(props["truthy"].type, Aurora::PropertyValue::Type::Bool);
    ASSERT_EQ(props["truthy"].asBool(), true);
    ASSERT_EQ(props["blooop"].type, Aurora::PropertyValue::Type::String);
    ASSERT_STREQ(props["blooop"].asString().c_str(), "blaap");
    ASSERT_EQ(props["bar"].type, Aurora::PropertyValue::Type::Float);
    ASSERT_EQ(props["bar"].asFloat(), 1.5f);
    ASSERT_EQ(props["v2"].type, Aurora::PropertyValue::Type::Float2);
    ASSERT_EQ(props["v2"].asFloat2().x, 1);
    ASSERT_EQ(props["v2"].asFloat2().y, 2);
    ASSERT_EQ(props["v3"].type, Aurora::PropertyValue::Type::Float3);
    ASSERT_EQ(props["v3"].asFloat3().x, 1);
    ASSERT_EQ(props["v3"].asFloat3().y, 2);
    ASSERT_EQ(props["v3"].asFloat3().z, 3);
    ASSERT_EQ(props["v4"].type, Aurora::PropertyValue::Type::Float4);
    ASSERT_EQ(props["v4"].asFloat4().x, 0.1f);
    ASSERT_EQ(props["v4"].asFloat4().y, 0.2f);
    ASSERT_EQ(props["v4"].asFloat4().z, 0.3f);
    ASSERT_EQ(props["v4"].asFloat4().w, 0.4f);
    ASSERT_EQ(props["mtx"].type, Aurora::PropertyValue::Type::Matrix4);
    ASSERT_EQ(props["mtx"].asMatrix4()[3][0], 4);
    ASSERT_EQ(props["mtx"].asMatrix4()[3][1], 5);
    ASSERT_EQ(props["mtx"].asMatrix4()[3][2], 6);

    ASSERT_EQ(props["v4"].hasValue(), true);
    props["v4"].clear();
    ASSERT_EQ(props["v4"].hasValue(), false);

    ASSERT_EQ(props["v3"].hasValue(), true);
    props["v3"] = nullptr;
    ASSERT_EQ(props["v3"].hasValue(), false);
}

TEST_F(PropertiesTest, ArrayTest)
{
    Aurora::Properties props = {
        {
            "myArrayProp",
            vector<string>({ "a", "b", "c" }),
        },
        {
            "emptyArrayProp",
            vector<string>(),
        },
    };

    ASSERT_STREQ(props["myArrayProp"].asStrings()[0].c_str(), "a");
    ASSERT_STREQ(props["myArrayProp"].asStrings()[1].c_str(), "b");
    ASSERT_STREQ(props["myArrayProp"].asStrings()[2].c_str(), "c");

    ASSERT_EQ(props["emptyArrayProp"].asStrings().size(), 0);

    props["emptyArrayProp"].asStrings().push_back("plop");
    ASSERT_STREQ(props["emptyArrayProp"].asStrings()[0].c_str(), "plop");

    props["anotherArray"] = vector<string>({ "foo", "bar" });
    ASSERT_STREQ(props["anotherArray"].asStrings()[0].c_str(), "foo");
    ASSERT_STREQ(props["anotherArray"].asStrings()[1].c_str(), "bar");
}

} // namespace

#endif
