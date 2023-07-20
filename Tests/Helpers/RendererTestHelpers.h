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

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

#include "BaselineImageHelpers.h"

using namespace std;

// GLM used by many tests
#define GLM_FORCE_CTOR_INIT
#pragma warning(push)
#pragma warning(disable : 4127) // nameless struct/union
#pragma warning(disable : 4201) // conditional expression is not constant
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/transform.hpp"
#pragma warning(pop)

/// Convenience helper functions for internal use in renderers unit tests
namespace TestHelpers
{

/// \brief Shared test fixture for all Renderers tests
class RendererTestBase : public testing::Test
{
public:
    RendererTestBase();
    ~RendererTestBase() {}

    /// Get the data path used for test assets.
    const std::string& dataPath() { return _dataPath; }

    /// \brief Runs a baseline image comparison test on the pixels
    /// \return the baseline image comparison result calculated from the rendered pixels
    TestHelpers::BaselineImageComparison::Result checkBaselineImage(const uint8_t* imageData,
        size_t width, size_t height, const std::string& folder = "",
        const std::string& filename = "");

    /// \brief Path used for baseline image tests to write baseline image files.
    const std::string& renderedBaselineImagePath() { return _renderedBaselineImagePath; }

    /// \brief Path used for baseline image tests to write output image files.
    const std::string& renderedOutputImagePath() { return _renderedOutputImagePath; }

    /// \brief Gets the name of current gtest for this fixture, combined with current renderer name.
    std::string currentTestName()
    {
        return std::string(testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    /// \brief Sets the threshold values used in baseline image tests
    /// carried out by renderAndCheckBaselineImage.
    /// \param pixelFailPercent Threshold for percentage pixel
    /// difference allowed before failure.
    /// \param maxPercentFailingPixels Max percentage of failing
    /// pixels allowed before comparison fails.
    /// \param pixelWarningPercent Threshold for percentage pixel
    /// difference allowed before warning.
    /// \param maxPercentWarningPixels Max percentage of warning
    /// pixels allowed before comparison fails.
    void setBaselineImageThresholds(float pixelFailPercent, float maxPercentFailingPixels,
        float pixelWarnPercent, float maxPercentWarningPixels);

    /// \brief Resets the threshold values used in baseline image tests carried out by
    /// renderAndCheckBaselineImage to their default values.
    void resetBaselineImageThresholdsToDefaults()
    {
        setBaselineImageThresholds(0.2f, 0.005f, 0.005f, 0.25f);
    }

    const std::string lastLogMessage() const { return _lastLogMessage; }
    int errorAndWarningCount() const { return _errorAndWarningCount; }

protected:
    std::string _dataPath;

    // Thresholds used for baseline image comparison in renderAndCheckBaselineImage
    TestHelpers::BaselineImageComparison::Thresholds baselineImageThresholds;

    // Path used for baseline image tests to write baseline image files
    std::string _renderedBaselineImagePath;

    // Path used for baseline image tests to write output image files
    std::string _renderedOutputImagePath;

    // Last log message received by Aurora::Foundation::Log logger.
    std::string _lastLogMessage;
    // Total number of log messages with level warning or greater received.
    int _errorAndWarningCount = 0;
};

// Convenience macro to render with renderAndCheckBaselineImage and ensure baseline image result is
// "Passed"
#define ASSERT_BASELINE_IMAGE_PASSES(_image, _width, _height)                                      \
    ASSERT_STREQ(checkBaselineImage(_image, _width, _height).toString().c_str(), "Passed")

} // namespace TestHelpers
