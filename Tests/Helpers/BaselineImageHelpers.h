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
#pragma once

#include "TestHelpers.h"

namespace TestHelpers
{

namespace BaselineImageComparison
{

// \brief Structure representing the pass/fail thresholds used in baseline image comparison
struct Thresholds
{
    /// Threshold for percentage pixel difference allowed before failure (1.0==100%)
    float pixelFailPercent = 0.1f;
    /// Max percentage of failing pixels allowed before comparison fails (1.0==100%)
    float maxPercentFailingPixels = 0.05f;
    /// Threshold for percentage pixel difference allowed before warning (1.0==100%)
    float pixelWarnPercent = 0.025f;
    /// Max percentage of warning pixels allowed before comparison fails (1.0==100%)
    float maxPercentWarningPixels = 0.2f;
};

// \brief Structure representing the results of a baseline image comparison
struct Result
{
    // \brief Enum representing status of comparison
    enum Status
    {
        Passed = 0,
        ComparisonFailed,
        NoBaselineImage,
        CouldNotCreateFolder,
        ImageSizeMismatch,
        FileIOError,
    };

    // \brief Construct a result from result enum and file names
    Result(Status res, const std::string& outputFile = "", const std::string& baselineFile = "",
        const Thresholds& thresholds = Thresholds()) :
        outputFileName(outputFile),
        baselineFileName(baselineFile),
        result(res),
        thresholds(thresholds)
    {
    }

    // \brief Output image file name used in comparison
    std::string outputFileName = "";
    // \brief Baseline image file name used in comparison
    std::string baselineFileName = "";
    // \brief Mean error over all pixels
    float meanError = 0.0f;
    // \brief Root mean-squared error over all pixels
    float rmsError = 0.0f;
    // \brief Peak signal-to-noise ratio over all pixels
    float PSNR = 0.0f;
    // \brief Max error over all pixels
    float maxError = 0.0f;
    // \brief Percentage of pixels failing comparison (1.0==100%)
    float percentFailingPixels = 0.0f;
    // \brief Percentage of pixels failing warning level comparison (1.0==100%)
    float percentWarningPixels = 0.0f;
    // \brief Comparison result
    Status result = ComparisonFailed;
    // \brief Thresholds used in comparison
    Thresholds thresholds;

    // \brief Did comparison pass?
    bool passed() { return result == Status::Passed; }

    // \brief Converts the results to a string
    std::string toString();
};

/// \brief Perform a baseline image comparison on the provided rendered pixels
/// \param renderedPixels Pointer to RGBA8 rendered pixels
/// \param width Width of rendered image
/// \param height Height of rendered image
/// \param baselineImageFolder Folder where baseline images are saved to,
/// should be in source repo and commited to Git (created if needed)
/// \param ouputImageFolder Folder where output images are saved to,
/// should be in temp folder not commited to Git (created if needed)
/// \param name Name used to create fileNames for output and baseline images
/// \param thresholds Threshold values that define pixel pass/fail criteria for test
/// \return Result of the comparison
Result compare(const uint8_t* renderedPixels, size_t width, size_t height,
    const std::string& baselineImageFolder, const std::string& ouputImageFolder,
    const std::string& name, const Thresholds& thresholds = Thresholds());

} // namespace BaselineImageComparison

} // namespace TestHelpers
