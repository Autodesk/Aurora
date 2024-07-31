// Copyright 2024 Autodesk, Inc.
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
#include <gtest/gtest.h>

#include "TestHelpers.h"

// USD headers required by HdAurora.
#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/aov.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/sdf/path.h>

// Aurora & glm headers.
#include <Aurora/Aurora.h>
#include <Aurora/Foundation/Timer.h>

// Internal HdAurora headers.
using namespace std;
PXR_NAMESPACE_USING_DIRECTIVE
#include "HdAuroraImageCache.h"
#include "HdAuroraRenderDelegate.h"

namespace
{
class StabilityTest : public ::testing::Test
{
public:
    StabilityTest() : _dataPath(TestHelpers::kSourceRoot + "/Tests/Assets") {}
    ~StabilityTest() {}

    // Optionally override the base class's setUp function to create some
    // data for the tests to use.
    void SetUp() override {}

    // Optionally override the base class's tearDown to clear out any changes
    // and release resources.
    void TearDown() override {}

protected:
    const std::string _dataPath;
};

// Illegal texture tests.
TEST_F(StabilityTest, TestTextures)
{
    // NOTE: In the following tests, all images will be treated as environment images since there
    // is no difference between processing material textures and environment maps when loading.
    std::unique_ptr<HdAuroraRenderDelegate> pAuroraRenderDelegate =
        std::make_unique<HdAuroraRenderDelegate>();

    // Test empty path. ///////////////////////////////////////////////////////
    auto resPath = pAuroraRenderDelegate->imageCache().acquireImage("", true, false);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();

    resPath = pAuroraRenderDelegate->imageCache().acquireImage("", true, true);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();

    // Test broken textures. //////////////////////////////////////////////////
    const std::string texFilename = _dataPath + "/Textures/invalid_image.jpg";

    resPath = pAuroraRenderDelegate->imageCache().acquireImage(texFilename, true, false);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();

    resPath = pAuroraRenderDelegate->imageCache().acquireImage(texFilename, true, true);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();

// TODO: To pass the extreme-sized tests, the corresponding fix in USD (hiooiio) is required.
// Enable the following tests once USD is upgraded.
#ifdef ENABLE_EXTREME_SIZE_TEST
    // Test extreme-sized textures. ///////////////////////////////////////////
    const std::string environmentImage = _dataPath + "/Textures/pretville_street_24k.exr";

    resPath = pAuroraRenderDelegate->imageCache().acquireImage(environmentImage, true, false);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();

    resPath = pAuroraRenderDelegate->imageCache().acquireImage(environmentImage, true, true);
    EXPECT_FALSE(resPath.empty());
    // Ensure no crash when resource activated.
    pAuroraRenderDelegate->setAuroraEnvironmentLightImagePath(resPath);
    pAuroraRenderDelegate->UpdateAuroraEnvironment();
#endif
}

} // namespace

#endif
