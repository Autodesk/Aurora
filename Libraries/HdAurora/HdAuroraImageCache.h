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

// Cache entry, use in cache map lookup.
struct HdAuroraImageCacheEntry
{
    // The pixels for the image.
    std::unique_ptr<uint8_t[]> pPixelData;
    // Total bytes of pixel data.
    size_t sizeInBytes;
    // The Aurora descriptor describing the image.
    Aurora::ImageDescriptor imageDesc;
};

// Image cache used by materials and environments to load Aurora images.
class HdAuroraImageCache
{
public:
    HdAuroraImageCache(Aurora::IScenePtr pAuroraScene) : _pAuroraScene(pAuroraScene) {}
    ~HdAuroraImageCache() {}

    // Set the flag to indicate the Y axis should be flipped on loaded images.
    void setIsYFlipped(bool val);

    // Acquire an image from the cache, loading if necessary.
    // Returns the Aurora path for the image (will be different for environment images.)
    Aurora::Path acquireImage(
        const string& sFilePath, bool isEnvironmentImage = false, bool linearize = false);

private:
    map<string, HdAuroraImageCacheEntry> _cache;
    Aurora::IScenePtr _pAuroraScene;
    bool _isYFlipped = true;
};
