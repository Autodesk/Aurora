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

#include "BaselineImageHelpers.h"

#include <iostream>
#include <sstream>
#include <string>

#include <Aurora/Foundation/Utilities.h>

using namespace std;

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#pragma warning(push)
#pragma warning(disable : 4459) // disable simd.h warning about 'cout' declaration.
#pragma warning(disable : 4566) // disable core.h warning about "micro" char type.
#pragma warning(disable : 4100) // disable simd.h warning about unreferenced formal parameters.
#pragma warning(disable : 4244) // disable simd.h warning about type conversion.
#pragma warning(disable : 4456) // disable farmhash.h warning about hiding local declaration.
#pragma warning(disable : 4267) // disable farmhash.h warning about type conversion.
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#pragma warning(pop)

#if __clang__
#pragma clang diagnostic push
#endif

namespace TestHelpers
{

namespace BaselineImageComparison
{

string Result::toString()
{
    // Create string from fileNames
    string fileStr = "Comparing " + outputFileName + " to " + baselineFileName;

    // Create string stream from contents of result struct
    stringstream statsStr;
    statsStr.precision(4);
    statsStr << fileStr << ", ";
    statsStr << "Failing pixels:" << static_cast<int>(percentFailingPixels * 100.0f)
             << "% must be less than "
             << static_cast<int>(thresholds.maxPercentFailingPixels * 100.0f)
             << "% to pass (Pixel fail threshold:"
             << static_cast<int>(thresholds.pixelFailPercent * 100.0f) << "%),";
    statsStr << "Warning pixels:" << static_cast<int>(percentWarningPixels * 100.0f)
             << "% must be less than "
             << static_cast<int>(thresholds.maxPercentWarningPixels * 100.0f)
             << "% to pass (Pixel warning threshold:"
             << static_cast<int>(thresholds.pixelWarnPercent * 100.0f) << "%),";
    statsStr << "Mean error:" << meanError << ", ";
    statsStr << "Max error:" << maxError << ", ";
    statsStr << "Root mean-squared error:" << rmsError << ", ";
    statsStr << "Peak signal-to-noise ratio:" << PSNR;

    string updateMessage =
        "\nRun tests with the environment variable UPDATE_BASELINE set to update the baseline "
        "images, and then commit them to git.\nBut *only* do so if the image changes are expected, "
        "DO NOT commit baseline images that have changed just to get tests to pass if you don't "
        "know why they have changed.\n";

    // Return different human readable string for every result type
    switch (result)
    {
    case Result::Passed:
        return "Passed";
    case Result::NoBaselineImage:
        return "No baseline image (" + fileStr + ")" + updateMessage;
    case Result::CouldNotCreateFolder:
        return "Could not create output or baseline image folder (" + fileStr + ")";
    case Result::FileIOError:
        return "File IO error (" + fileStr + ")";
    case Result::ImageSizeMismatch:
        return "Baseline and output image sizes do not match (" + fileStr + ")";
    default:
        return "Failed (" + statsStr.str() + ")" + updateMessage;
    }
}

// Convenience function to write image from OIIO image buffer
static bool writeBuffer(const OIIO::ImageBuf& buffer, const string& fileName)
{
    // Create the OIIO output image
    unique_ptr<OIIO::ImageOutput> pImgOut = OIIO::ImageOutput::create(fileName);

    // Return false if failed
    if (!pImgOut)
        return false;

    // Open the OIIO output image (return false if failed)
    // Flawfinder: ignore (Bug in Flawfinder which flags anything called "open", see OGSMOD-832)
    if (!pImgOut->open(fileName.c_str(), buffer.spec()))
        return false;

    // Write the OIIO buffer to the output image (return false if failed)
    if (!buffer.write(pImgOut.get()))
        return false;

    // Close the output image (return false if failed)
    if (!pImgOut->close())
        return false;

    return true;
}

Result compare(const uint8_t* renderedPixels, size_t width, size_t height,
    const string& baselineImageFolder, const string& ouputImageFolder, const string& name,
    const Thresholds& thresholds)
{
    // All baseline images are 8-bit RGBA
    const int channels = 4;

    // Total number of pixels in rendered image
    size_t totalPixels = width * height;

    // Create PNG image file name from test name
    string fileName = name + ".png";

    // replace non-file chars with underscore
    Aurora::Foundation::sanitizeFileName(fileName);

    // Output and baseline file named identically in baseline and output folder
    string outputFile   = combinePaths(ouputImageFolder, fileName);
    string baselineFile = combinePaths(baselineImageFolder, fileName);

    // Create any folders that don't exist
    if (!TestHelpers::createDirectory(ouputImageFolder) ||
        !TestHelpers::createDirectory(baselineImageFolder))
    {
        // Return error if folder creation failed
        return Result(Result::Status::CouldNotCreateFolder, outputFile, baselineFile);
    }

    // Create OIIO region of interest and spec for output and baseline image
    OIIO::ROI roi(0, static_cast<int>(width), 0, static_cast<int>(height));
    OIIO::ImageSpec spec(
        static_cast<int>(width), static_cast<int>(height), channels, OIIO::TypeDesc::UINT8);

    // Create OIIO image buffer from rendered pixels
    OIIO::ImageBuf outputImageBuffer;
    outputImageBuffer.reset(spec);

    // Set pixels from rendered image
    if (!outputImageBuffer.set_pixels(roi, OIIO::TypeDesc::UCHAR, renderedPixels))
    {
        // If setting pixels fails return immediate with file IO error
        return Result(Result::Status::FileIOError, outputFile, baselineFile);
    }

    // Write the output image to the output folder
    if (!writeBuffer(outputImageBuffer, outputFile))
    {
        // If writing fails return immediate with file IO error
        return Result(Result::Status::FileIOError, outputFile, baselineFile);
    }

    // Create the return result structure
    Result res(Result::Status::Passed, outputFile, baselineFile, thresholds);

    // If no baseline file set error field
    if (!TestHelpers::fileIsReadable(baselineFile))
    {
        res.result = Result::Status::NoBaselineImage;
    }
    else
    {
        // Read back the output file into an OIIO image buffer.
        // TODO: The comparison fails when the output image has an alpha channel, unless it is
        // reloaded like this. The comparison *should* succeed without having to do this.
        OIIO::ImageBuf readbackOutputImageBuffer;
        readbackOutputImageBuffer.reset(outputFile);
        // Flawfinder: ignore (Bug in Flawfinder which flags anything called "read", see OGSMOD-832)
        if (!readbackOutputImageBuffer.read())
        {
            return Result(Result::Status::FileIOError, outputFile, baselineFile);
        }

        // Read the baseline file into an OIIO image buffer.
        OIIO::ImageBuf baselineImageBuffer;
        baselineImageBuffer.reset(baselineFile);
        // Flawfinder: ignore (Bug in Flawfinder which flags anything called "read", see OGSMOD-832)
        if (!baselineImageBuffer.read())
        {
            return Result(Result::Status::FileIOError, outputFile, baselineFile);
        }

        // Ensure output and baseline image are the same size
        auto baselineSpec = baselineImageBuffer.spec();
        if (baselineSpec.width != static_cast<int>(width) ||
            baselineSpec.height != static_cast<int>(height))
            res.result = Result::Status::ImageSizeMismatch;
        else
        {

            // Run the OIIO comparison algorithm on the two images
            auto compRes = OIIO::ImageBufAlgo::compare(readbackOutputImageBuffer,
                baselineImageBuffer, thresholds.pixelFailPercent, thresholds.pixelWarnPercent);

            // Copy the OIIO comparison result to our result structure
            res.percentFailingPixels = compRes.nfail / (float)totalPixels;
            res.percentWarningPixels = compRes.nwarn / (float)totalPixels;
            res.PSNR                 = static_cast<float>(compRes.PSNR);
            res.meanError            = static_cast<float>(compRes.meanerror);
            res.maxError             = static_cast<float>(compRes.maxerror);
            res.rmsError             = static_cast<float>(compRes.rms_error);

            // If more failing pixels than allowed set failure result
            if (res.percentFailingPixels > thresholds.maxPercentFailingPixels)
                res.result = Result::Status::ComparisonFailed;

            // If more warning pixels than allowed set failure result
            if (res.percentWarningPixels > thresholds.maxPercentWarningPixels)
                res.result = Result::Status::ComparisonFailed;
        }
    }

    // If baseline image comparison failed, but UPDATE_BASELINE env var is set, update the baseline
    // image with the new output image. Unless FORCE_UPDATE_BASELINE is set we only do this if it
    // failed, as otherwise the fuzzy comparison will mean all the images are replaced, not just the
    // ones that have significantly changed
    // Flawfinder: ignore (getenv perfectly safe to use in unit tests)
    if ((!res.passed() && TestHelpers::getFlagEnvironmentVariable("UPDATE_BASELINE")) ||
        TestHelpers::getFlagEnvironmentVariable("FORCE_UPDATE_BASELINE"))
    {
        // Inform user that the baseline was updated
        cout << "UPDATE_BASELINE env. var. is set. Generating new baseline image:" << baselineFile
             << endl;

        // Overwrite the old baseline image with the new output
        if (!writeBuffer(outputImageBuffer, baselineFile))
            return Result(Result::Status::FileIOError, outputFile, baselineFile);

        // If we are updating the test always passes (as the baseline image IS the output image now)
        return Result(Result::Status::Passed, outputFile, baselineFile);
    }

    return res;
}

} // namespace BaselineImageComparison

} // namespace TestHelpers
