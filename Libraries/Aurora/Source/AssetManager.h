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

BEGIN_AURORA

/// Image asset loaded by AssetManager::acquireImage.
struct ImageAsset
{
    // The Aurora image data structure.
    IImage::InitData data;

    // The pixels referenced image data.
    unique_ptr<unsigned char[]> pixels;

    // Size in bytes of the pixel data.
    size_t sizeBytes;
};

/// Process image function, used by asset manager to process image pixels from a raw buffer.
///
/// \param buffer The raw buffer containing the unprocessed image data.
/// \param filename The filename of the image resource.
/// \param pImageOut The processed image data.
/// \param forceLinear If true the image will be loaded with a linear color space, otherwise
/// inferred from image metadata.
/// \return True if loaded successfully.
using ProcessImageFunction = function<bool(
    const vector<unsigned char>& buffer, const string& filename, ImageAsset* pImageOut)>;

/// Asset manager class, used to load and cache external asset files.
class AssetManager
{
public:
    /// Constructor.
    ///
    /// \param loadResourceFunction Function used to load resource buffers from a URI.
    /// \param processImageFunction Function used to process a image data after loading.
    AssetManager(LoadResourceFunction loadResourceFunction = nullptr,
        ProcessImageFunction processImageFunction          = nullptr);

    /// Load a new text file from a Universal Resource Identifier(URI) string, or return existing
    /// one if already loaded.
    shared_ptr<string> acquireTextFile(const string& uri);

    /// Load a new image from a Universal Resource Identifier(URI) string, or return existing
    /// one if already loaded.
    shared_ptr<ImageAsset> acquireImage(const string& uri);

    /// Set the global flag to enable flipping images vertically in the default image decoding
    /// function
    /// \param enabled If true image rows loaded bottom-to-top.
    void enableVerticalFlipOnImageLoad(bool enabled) { _flipImageY = enabled; }

    /// Set the callback function used to load all resources from a provided URI.
    ///
    /// \param The callback function to used for all resource loading.
    void setLoadResourceFunction(LoadResourceFunction func) { _loadResourceFunction = func; }

protected:
    // Flipped vertically defaults to true, this matches traditional Aurora.
    bool _flipImageY = true;

    LoadResourceFunction _loadResourceFunction;
    ProcessImageFunction _processImageFunction;
};

END_AURORA
