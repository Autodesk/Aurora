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

    _cache[auroraImagePath]           = HdAuroraImageCacheEntry();
    HdAuroraImageCacheEntry& newEntry = _cache[auroraImagePath];
    newEntry.imageDesc.getPixelData   = [this, auroraImagePath](
                                          Aurora::PixelData& dataOut, glm::ivec2, glm::ivec2) {
        const HdAuroraImageCacheEntry& entry = _cache[auroraImagePath];
        dataOut.address                      = entry.pPixelData.get();
        dataOut.size                         = entry.sizeInBytes;
        return true;
    };
    newEntry.imageDesc.isEnvironment = isEnvironmentImage;
    newEntry.imageDesc.linearize     = !forceLinear;

    std::string resolvedPath           = ArGetResolver().CreateIdentifier(sFilePath);
    pxr::HioImageSharedPtr const image = pxr::HioImage::OpenForReading(resolvedPath);
    if (image)
    {
        pxr::HioImage::StorageSpec imageData;
        auto hioFormat    = image->GetFormat();
        imageData.width   = image->GetWidth();
        imageData.height  = image->GetHeight();
        imageData.depth   = 1;
        imageData.flipped = _isYFlipped;
        imageData.format  = image->GetFormat();
        bool paddingRequired =
            hioFormat == HioFormatUNorm8Vec3srgb || hioFormat == HioFormatUNorm8Vec3;
        unique_ptr<uint8_t[]> pTempPixels;
        if (paddingRequired)
        {
            hioFormat            = hioFormat == HioFormatUNorm8Vec3srgb ? HioFormatUNorm8Vec4srgb
                                                                        : HioFormatUNorm8Vec4;
            newEntry.sizeInBytes = image->GetWidth() * image->GetHeight() * 4;
            pTempPixels.reset(
                new uint8_t[image->GetWidth() * image->GetHeight() * image->GetBytesPerPixel()]);
            newEntry.pPixelData.reset(new uint8_t[newEntry.sizeInBytes]);
            imageData.data = pTempPixels.get();
        }
        else
        {
            newEntry.sizeInBytes =
                image->GetWidth() * image->GetHeight() * image->GetBytesPerPixel();
            newEntry.pPixelData.reset(new uint8_t[newEntry.sizeInBytes]);
            imageData.data = newEntry.pPixelData.get();
        }

        bool res = image->Read(imageData);
        if (res)
        {
            newEntry.imageDesc.width  = image->GetWidth();
            newEntry.imageDesc.height = image->GetHeight();
            if (paddingRequired)
            {
                for (size_t idx = 0; idx < newEntry.imageDesc.width * newEntry.imageDesc.height;
                     idx++)
                {
                    newEntry.pPixelData[idx * 4 + 0] = pTempPixels[idx * 3 + 0];
                    newEntry.pPixelData[idx * 4 + 1] = pTempPixels[idx * 3 + 1];
                    newEntry.pPixelData[idx * 4 + 2] = pTempPixels[idx * 3 + 2];
                    newEntry.pPixelData[idx * 4 + 3] = 0xFF;
                }
            }

            switch (hioFormat)
            {
            case HioFormatUNorm8Vec4srgb:
                // Set linearize flag, unless force linear is true.
                newEntry.imageDesc.linearize = !forceLinear;
                // Fall through...
            case HioFormatUNorm8Vec4:
                newEntry.imageDesc.format = Aurora::ImageFormat::Integer_RGBA;
                break;
            case HioFormatFloat32Vec3:
                newEntry.imageDesc.format = Aurora::ImageFormat::Float_RGB;
                break;
            case HioFormatFloat32Vec4:
                newEntry.imageDesc.format = Aurora::ImageFormat::Float_RGBA;
                break;
            case HioFormatUNorm8:
                newEntry.imageDesc.format = Aurora::ImageFormat::Byte_R;
                break;
            default:
                AU_ERROR("%s: Unsupported image format:%x", sFilePath.c_str(), image->GetFormat());
                newEntry.pPixelData.reset();
                break;
            }
        }
        else
            newEntry.pPixelData.reset();
    }
    if (!newEntry.pPixelData)
    {
        AU_ERROR("Failed to load image image :%s, using placeholder", sFilePath.c_str());
        newEntry.sizeInBytes = 8;
        newEntry.pPixelData.reset(new uint8_t[8]);
        memset(newEntry.pPixelData.get(), 0xff, newEntry.sizeInBytes);
        newEntry.pPixelData[0]    = 0xff;
        newEntry.imageDesc.width  = 2;
        newEntry.imageDesc.height = 1;
        newEntry.imageDesc.format = Aurora::ImageFormat::Integer_RGBA;
    }

    _pAuroraScene->setImageDescriptor(auroraImagePath, newEntry.imageDesc);

    return auroraImagePath;
}
