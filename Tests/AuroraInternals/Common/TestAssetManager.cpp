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

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

#include "AssetManager.h"

namespace
{

// Test fixture accessing test asset data path.
class AssetManagerTest : public ::testing::Test
{
public:
    AssetManagerTest() : _dataPath(TestHelpers::kSourceRoot + "/Tests/Assets") {}
    ~AssetManagerTest() {}
    const std::string& dataPath() { return _dataPath; }

protected:
    std::string _dataPath;
};

// Basic asset manager test.
TEST_F(AssetManagerTest, BasicTest)
{
    Aurora::AssetManager testMgr;

    // Load image and ensure success.
    auto imgData = testMgr.acquireImage(dataPath() + "/Textures/fishscale_basecolor.jpg");
    ASSERT_NE(imgData, nullptr);

    // Load text file and ensure success.
    auto textData = testMgr.acquireTextFile(dataPath() + "/Materials/FishScale.mtlx");
    ASSERT_NE(textData, nullptr);
}

TEST_F(AssetManagerTest, LinearImageTest)
{
    Aurora::AssetManager testMgr;

    // Load sRGB image (must be linearized)
    auto imgData0 = testMgr.acquireImage(dataPath() + "/Textures/fishscale_basecolor.jpg");
    ASSERT_NE(imgData0, nullptr);
    ASSERT_EQ(imgData0->data.linearize, true);

    // Load linear image (should not be linearized)
    auto imgData1 = testMgr.acquireImage(dataPath() + "/Textures/fishscale_normal.png");
    ASSERT_NE(imgData1, nullptr);
    ASSERT_EQ(imgData1->data.linearize, false);
}

} // namespace

#endif
