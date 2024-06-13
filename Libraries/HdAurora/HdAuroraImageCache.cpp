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
#include "pch.h"

#include "HdAuroraImageCache.h"

// Flag to set the flip-y flag on loaded images.
void HdAuroraImageCache::setIsYFlipped(bool val)
{
    _isYFlipped = val;
}

Aurora::Path HdAuroraImageCache::acquireImage(
    const string& sFilePath, bool isEnvironmentImage, bool forceLinear)
{
    // Calculate an Aurora image path from file path (should be different for environment images.)
    Aurora::Path auroraImagePath =
        isEnvironmentImage ? "HdAuroraEnvImage:" + sFilePath : "HdAuroraImage:" + sFilePath;
    if (forceLinear)
        auroraImagePath += "-linear";

    auto iter = _cache.find(auroraImagePath);
    if (iter != _cache.end())
        return auroraImagePath;

    _cache.insert(auroraImagePath);

    Aurora::ImageDescriptor descriptor;
    descriptor.getData = [this, auroraImagePath, sFilePath, forceLinear](
                             Aurora::ImageData& dataOut, Aurora::AllocateBufferFunction alloc) {
        // Get resolved image path from ArResolver.
        std::string resolvedPath = ArGetResolver().CreateIdentifier(sFilePath);

        // Load image using resolved path, return false if none found.
        pxr::HioImageSharedPtr const image = pxr::HioImage::OpenForReading(resolvedPath);
        if (!image)
        {
            AU_ERROR("Failed to open image file %s with resolved path %s", sFilePath.c_str(),
                resolvedPath.c_str());
            return false;
        }

        // Fill in HIO storage struct.
        pxr::HioImage::StorageSpec imageData;
        auto hioFormat    = image->GetFormat();
        imageData.width   = image->GetWidth();
        imageData.height  = image->GetHeight();
        imageData.depth   = 1;
        imageData.flipped = _isYFlipped;
        imageData.format  = image->GetFormat();

        // NOTE: Multiplying int values may exceed INT_MAX. Cast to size_t for security.
        const size_t widthSizeT  = image->GetWidth();
        const size_t heightSizeT = image->GetHeight();
        const size_t bppSizeT    = image->GetBytesPerPixel();
        bool paddingRequired =
            hioFormat == HioFormatUNorm8Vec3srgb || hioFormat == HioFormatUNorm8Vec3;

        // If we need padding create working buffer to read into, otherwise read straight into the
        // pixel buffer.
        unique_ptr<uint8_t[]> pUnpaddedPixels;
        // Get temp pixels for image.
        uint8_t* pPixelData;
        if (paddingRequired)
        {
            hioFormat = hioFormat == HioFormatUNorm8Vec3srgb ? HioFormatUNorm8Vec4srgb
                                                             : HioFormatUNorm8Vec4;

            const size_t newPixelSize = 4;
            dataOut.bufferSize        = widthSizeT * heightSizeT * newPixelSize;
            pUnpaddedPixels.reset(new uint8_t[widthSizeT * heightSizeT * bppSizeT]);
            imageData.data = pUnpaddedPixels.get();
            pPixelData     = static_cast<uint8_t*>(alloc(dataOut.bufferSize));
        }
        else
        {
            dataOut.bufferSize = widthSizeT * heightSizeT * bppSizeT;
            pPixelData         = static_cast<uint8_t*>(alloc(dataOut.bufferSize));
            imageData.data     = pPixelData;
        }

        // Set output data point to the pixel buffer.
        dataOut.pPixelBuffer = pPixelData;

        // Set output image dimensions.
        dataOut.dimensions = { image->GetWidth(), image->GetHeight() };

        // Read the pixels.
        bool res = image->Read(imageData);
        if (!res)
        {
            AU_ERROR("Failed to read image file %s with resolved path %s", sFilePath.c_str(),
                resolvedPath.c_str());
            return false;
        }

        // Pad to RGBA if required.
        if (paddingRequired)
        {
            const size_t totalSize = static_cast<size_t>(image->GetWidth()) *
                static_cast<size_t>(image->GetWidth());
            for (size_t idx = 0; idx < totalSize; idx++)
            {
                pPixelData[idx * 4 + 0] = pUnpaddedPixels[idx * 3 + 0];
                pPixelData[idx * 4 + 1] = pUnpaddedPixels[idx * 3 + 1];
                pPixelData[idx * 4 + 2] = pUnpaddedPixels[idx * 3 + 2];
                pPixelData[idx * 4 + 3] = 0xFF;
            }
        }

        switch (hioFormat)
        {
        case HioFormatUNorm8Vec4srgb:
            // Set linearize flag, unless force linear is true.
            dataOut.overrideLinearize = !forceLinear;
            dataOut.linearize         = true;
            // Fall through...
        case HioFormatUNorm8Vec4:
            dataOut.format = Aurora::ImageFormat::Integer_RGBA;
            break;
        case HioFormatFloat32Vec3:
            dataOut.format = Aurora::ImageFormat::Float_RGB;
            break;
        case HioFormatFloat32Vec4:
            dataOut.format = Aurora::ImageFormat::Float_RGBA;
            break;
        case HioFormatUNorm8:
            dataOut.format = Aurora::ImageFormat::Byte_R;
            break;
        default:
            AU_ERROR("%s: Unsupported image format:%x", sFilePath.c_str(), image->GetFormat());
            return false;
            break;
        }

        return true;
    };
    descriptor.isEnvironment = isEnvironmentImage;
    descriptor.linearize     = forceLinear;

    _pAuroraScene->setImageDescriptor(auroraImagePath, descriptor);

    return auroraImagePath;
}
