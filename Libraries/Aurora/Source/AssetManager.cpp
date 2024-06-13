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

#include "AssetManager.h"

// Include STB for image loading.
// TODO: Image loading will eventually be handled by clients.
#pragma warning(push)
// Disabe type conversion warnings intruduced from stb master.
// refer to the commit in stb
// https://github.com/nothings/stb/commit/b15b04321dfd8a2307c49ad9c5bf3c0c6bcc04cc
#pragma warning(disable : 4244)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma warning(pop)

BEGIN_AURORA

// Returns true if the image uses the sRGB color space, and false if it is linear.
// NOTE: This will only return false if the image is a PNG, with no sRGB tag, and gamma set to 1.0.
// TODO: We should use ImageManager for this in a client callback, rather than using STBI internal
// functions, but that is not easily done using OIIO or the current ImageManager API interface.
bool isImageSRGB(stbi_uc const* buffer, int len)
{
    // Set up STBI context.
    stbi__context s;
    stbi__start_mem(&s, buffer, len);

    // Return true if not a PNG file. Non-PNG images assumed to be sRGB.
    if (!stbi__png_test(&s))
    {
        return true;
    }

    // Setup PNG reading structure.
    [[maybe_unused]] stbi__png p;
    p.s        = &s;
    p.expanded = nullptr;
    p.idata    = nullptr;
    p.out      = nullptr;

    // Return and print error if header not parsed.
    if (!stbi__check_png_header(&s))
    {
        AU_ERROR("Failed to parse PNG header");
        return true;
    }

    // Default to gamma of -1 (invalid)
    int gamma = -1;

    // Maximum number of chunk in header (avoid infinite loop)
    int kHeaderMaxChunks = 0xFFFF;

    // Iterate through chunks until image data is found.
    for (int i = 0; i < kHeaderMaxChunks; i++)
    {
        // Read next chunk.
        stbi__pngchunk c = stbi__get_chunk_header(&s);

        // Process based on types.
        switch (c.type)
        {
        case STBI__PNG_TYPE('s', 'R', 'G', 'B'):
        {
            // Return true if "sRGB" chunk found (overrides gamma chunk regardless of value.)
            return true;
        }
        case STBI__PNG_TYPE('g', 'A', 'M', 'A'):
        {
            // Set the gamma from "gAMA" chunk.
            gamma = stbi__get32be(&s);
            break;
        }
        case STBI__PNG_TYPE('I', 'D', 'A', 'T'):
        {
            // This is the "IDAT" chunk, so we've reached the image data at the end of the header,
            // and we can return the result. PNG is considered linear if the gamma is exactly 100000
            // (meaning gamma of 1.0).
            return gamma != 100000;
        }
        default:
        {
            // Skip other chunks.
            stbi__skip(&s, c.length);
            break;
        }
        }

        // Read the checksum at the end of the chunk.
        stbi__get32be(&s);
    }

    // This should never be reached, because reading should stop at the IDAT chunk (above).
    AU_ERROR("Error parsing PNG");

    return true;
}

// Default load resource function.
bool defaultLoadResourceFunction(
    const string& uri, vector<unsigned char>* pBufferOut, string* pFileNameOut)
{
    // Default load resource function just loads URI as file path directly.
    ifstream is(uri, ifstream::binary);
    if (!is)
        return false;
    is.seekg(0, is.end);
    size_t length = is.tellg();
    is.seekg(0, is.beg);
    pBufferOut->resize(length);
    is.read((char*)&(*pBufferOut)[0], length);

    // Copy the URI to file name with no manipulation.
    *pFileNameOut = uri;

    return true;
}

// Default process image function.
// Has extra flipImageY argument that must be filled in.
bool defaultProcessImageFunction(const vector<unsigned char>& buffer, const string& filename,
    ImageAsset* pImageOut, bool flipImageY)
{
    // Use STB to decode the image buffer.
    // TODO: Image loading will eventually be handled by clients.
    int width, height, components;

    // Is this image HDR?
    bool isHDR = stbi_is_hdr_from_memory(&buffer[0], (int)buffer.size());

    if (isHDR)
    {
        // Don't flip HDR images.
        stbi_set_flip_vertically_on_load(false);

        // Load HDR image as floats.
        float* pPixels =
            stbi_loadf_from_memory(&buffer[0], (int)buffer.size(), &width, &height, &components, 0);

        AU_ASSERT(
            components == 3 || components == 4, "Only RGB and RGBA HDR images currently supported");

        // Fill in Aurora image data struct.
        pImageOut->data.width     = width;
        pImageOut->data.height    = height;
        pImageOut->data.name      = filename;
        pImageOut->data.linearize = false;
        pImageOut->data.format = components == 3 ? ImageFormat::Float_RGB : ImageFormat::Float_RGBA;
        size_t sizeBytes       = static_cast<size_t>(width * height * components * sizeof(float));
        pImageOut->pixels      = make_unique<unsigned char[]>(sizeBytes);
        pImageOut->data.pImageData = pImageOut->pixels.get();
        pImageOut->sizeBytes       = sizeBytes;

        // Copy pixels.
        memcpy(pImageOut->pixels.get(), pPixels, sizeBytes);

        // Free the pixels allocated by STB.
        stbi_image_free(pPixels);
    }
    else
    {

        // Set the flipped flag based on static variable in AssetManager.
        stbi_set_flip_vertically_on_load(flipImageY);

        // Load LDR image as bytes.
        unsigned char* pPixels =
            stbi_load_from_memory(&buffer[0], (int)buffer.size(), &width, &height, &components, 0);

        // Check if image is sRGB (if so set linearize flag.)
        bool isSRGB = isImageSRGB(&buffer[0], (int)buffer.size());

        // Fill in Aurora image data struct.
        pImageOut->data.width      = width;
        pImageOut->data.height     = height;
        pImageOut->data.name       = filename;
        pImageOut->data.linearize  = isSRGB;
        pImageOut->data.format     = ImageFormat::Integer_RGBA;
        size_t sizeBytes           = static_cast<size_t>(width * height * 4);
        pImageOut->pixels          = make_unique<unsigned char[]>(sizeBytes);
        pImageOut->data.pImageData = pImageOut->pixels.get();
        pImageOut->sizeBytes       = sizeBytes;

        // Output images must have four components (RGBA) so fill out missing components as needed.
        if (components == 1)
        {
            // Single-component (R) image: duplicate the R component to G and B, and set alpha to
            // the maximum value.
            unsigned char* pIn  = pPixels;
            unsigned char* pOut = pImageOut->pixels.get();
            for (int i = 0; i < width * height; i++)
            {
                unsigned char chIn = *(pIn++);
                *(pOut++)          = chIn;
                *(pOut++)          = chIn;
                *(pOut++)          = chIn;
                *(pOut++)          = 255;
            }
        }
        else if (components == 3)
        {
            // RGB image: set alpha to the maximum value.
            unsigned char* pIn  = pPixels;
            unsigned char* pOut = pImageOut->pixels.get();
            for (int i = 0; i < width * height; i++)
            {
                *(pOut++) = *(pIn++);
                *(pOut++) = *(pIn++);
                *(pOut++) = *(pIn++);
                *(pOut++) = 255;
            }
        }
        else if (components == 4)
        {
            // Just copy directly if the image already has RGBA components.
            memcpy(pImageOut->pixels.get(), pPixels, sizeBytes);
        }
        else
        {
            // Fail if unsupported component count.
            AU_FAIL("%s invalid number of components %d", filename.c_str(), components);
        }

        // Free the pixels allocated by STB.
        stbi_image_free(pPixels);
    }

    return true;
};

AssetManager::AssetManager(
    LoadResourceFunction loadResourceFunction, ProcessImageFunction processImageFunction)
{
    // Set the load resource callback, use the default if argument is null.
    _loadResourceFunction =
        loadResourceFunction ? loadResourceFunction : defaultLoadResourceFunction;

    // Set the process image callback, use the default (getting the value of flipImageY from
    // member variable) if argument is null.
    if (processImageFunction)
        _processImageFunction = processImageFunction;
    else
        _processImageFunction = [this](const vector<unsigned char>& buffer, const string& filename,
                                    ImageAsset* pImageOut) {
            return defaultProcessImageFunction(buffer, filename, pImageOut, _flipImageY);
        };
}

shared_ptr<string> AssetManager::acquireTextFile(const string& uri)
{
    // Use callback function to load buffer.
    vector<unsigned char> buffer;
    string filename;
    if (!_loadResourceFunction(uri, &buffer, &filename))
        return nullptr;

    // Return shared pointer to buffer as string.
    // TODO: Should cache based on URI.
    return make_shared<string>((char*)&buffer[0], buffer.size());
}

shared_ptr<ImageAsset> AssetManager::acquireImage(const string& uri)
{
    // Use callback function to load buffer.
    vector<unsigned char> buffer;
    string filename;
    if (!_loadResourceFunction(uri, &buffer, &filename))
        return nullptr;

    // Create shared pointer to image asset.
    // TODO: Should cache based on URI.
    shared_ptr<ImageAsset> pImageData = make_shared<ImageAsset>();

    // Process the raw image using callback function to produce Aurora image data.
    if (!_processImageFunction(buffer, filename, pImageData.get()))
        return nullptr;

    // Return shared pointer.
    return pImageData;
}

END_AURORA
