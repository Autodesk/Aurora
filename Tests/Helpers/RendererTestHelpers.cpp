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

#include "RendererTestHelpers.h"

#include <Aurora/Foundation/Log.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

using namespace Aurora::Foundation;

namespace TestHelpers
{

RendererTestBase::RendererTestBase() :
    _dataPath(TestHelpers::kSourceRoot + "/Tests/Data"),
    // Default baseline image location is in the Aurora Tests folder (should be committed to Git)
    _renderedBaselineImagePath(TestHelpers::kSourceRoot + "/Tests/Core/BaselineImages"),
    // Default baseline image location is in local working folder (should not be committed to Git)
    _renderedOutputImagePath("./OutputImages")
{

    // Set the default baseline image comparison thresholds.
    resetBaselineImageThresholdsToDefaults();
}

TestHelpers::BaselineImageComparison::Result RendererTestBase::checkBaselineImage(
    const uint8_t* imageData, size_t width, size_t height, const std::string& folder,
    const std::string& filename)
{
    string name =
        filename.empty() ? testing::UnitTest::GetInstance()->current_test_info()->name() : filename;

    // Ensure baseline and output folder are created.
    TestHelpers::createDirectory(_renderedBaselineImagePath);
    TestHelpers::createDirectory(_renderedOutputImagePath);

    // Concatenate folder, if non-zero length.
    std::string baselinePath =
        folder.size() ? _renderedBaselineImagePath + "/" + folder : _renderedBaselineImagePath;
    // Concatenate folder, if non-zero length.
    std::string outputPath =
        folder.size() ? _renderedOutputImagePath + "/" + folder : _renderedOutputImagePath;

    // run a baseline image comparison on the result (using current thresholds from this fixture.)
    return TestHelpers::BaselineImageComparison::compare(
        imageData, width, height, baselinePath, outputPath, name, baselineImageThresholds);
}

void RendererTestBase::setBaselineImageThresholds(float pixelFailPercent,
    float maxPercentFailingPixels, float pixelWarnPercent, float maxPercentWarningPixels)
{
    // Set the thresholds used in subsequent calls to renderAndCheckBaselineImage
    baselineImageThresholds.pixelFailPercent        = pixelFailPercent;
    baselineImageThresholds.maxPercentFailingPixels = maxPercentFailingPixels;
    baselineImageThresholds.pixelWarnPercent        = pixelWarnPercent;
    baselineImageThresholds.maxPercentWarningPixels = maxPercentWarningPixels;
}

} // namespace TestHelpers
