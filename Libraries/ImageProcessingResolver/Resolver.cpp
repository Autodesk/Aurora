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

#include "ConvertEnvMapLayout.h"
#include "Linearize.h"
#include "Resolver.h"

#define URI_STATIC_BUILD 1
#include "uriparser/Uri.h"

#define TINYEXR_USE_MINIZ 1
#define TINYEXR_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4706)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "tinyexr.h"
#pragma clang diagnostic pop
#pragma warning(pop)

#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

using namespace pxr;

HioFormat convertToFloat(HioFormat format)
{
    switch (format)
    {
    case HioFormatUNorm8:
        return HioFormatFloat32;
    case HioFormatUNorm8Vec2:
        return HioFormatFloat32Vec2;
    case HioFormatUNorm8Vec3:
        return HioFormatFloat32Vec3;
    case HioFormatUNorm8Vec4:
        return HioFormatFloat32Vec4;

    case HioFormatUNorm8srgb:
        return HioFormatFloat32;
    case HioFormatUNorm8Vec2srgb:
        return HioFormatFloat32Vec2;
    case HioFormatUNorm8Vec3srgb:
        return HioFormatFloat32Vec3;
    case HioFormatUNorm8Vec4srgb:
        return HioFormatFloat32Vec4;

    case HioFormatSNorm8:
        return HioFormatFloat32;
    case HioFormatSNorm8Vec2:
        return HioFormatFloat32Vec2;
    case HioFormatSNorm8Vec3:
        return HioFormatFloat32Vec3;
    case HioFormatSNorm8Vec4:
        return HioFormatFloat32Vec4;

    case HioFormatFloat16:
        return HioFormatFloat32;
    case HioFormatFloat16Vec2:
        return HioFormatFloat32Vec2;
    case HioFormatFloat16Vec3:
        return HioFormatFloat32Vec3;
    case HioFormatFloat16Vec4:
        return HioFormatFloat32Vec4;

    case HioFormatFloat32:
        return HioFormatFloat32;
    case HioFormatFloat32Vec2:
        return HioFormatFloat32Vec2;
    case HioFormatFloat32Vec3:
        return HioFormatFloat32Vec3;
    case HioFormatFloat32Vec4:
        return HioFormatFloat32Vec4;
    default:
        AU_FAIL("Unsupported format.");
    }
    return HioFormatCount;
}

OIIO::TypeDesc::BASETYPE convertToOIIODataType(pxr::HioType type)
{
    OIIO::TypeDesc::BASETYPE oiioType = OIIO::TypeDesc::UNKNOWN;
    switch (type)
    {
    case pxr::HioTypeUnsignedByte:
        oiioType = OIIO::TypeDesc::UINT8;
        break;
    case pxr::HioTypeUnsignedShort:
        oiioType = OIIO::TypeDesc::UINT16;
        break;
    case pxr::HioTypeUnsignedInt:
        oiioType = OIIO::TypeDesc::UINT32;
        break;
    case pxr::HioTypeHalfFloat:
        oiioType = OIIO::TypeDesc::HALF;
        break;
    case pxr::HioTypeFloat:
        oiioType = OIIO::TypeDesc::FLOAT;
        break;
    case pxr::HioTypeDouble:
        oiioType = OIIO::TypeDesc::DOUBLE;
        break;
    default:
        oiioType = OIIO::TypeDesc::UNKNOWN;
        break;
    }
    return oiioType;
}

// Asset path prefix, used once asset URI is processed.
std::string assetPathPrefix = "@@@ImageProcessingAsset_";

// Shink an image to the provided dimensions.
// imageData and imageBuf will be modified to point to the new image.
void shrinkImage(
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf, int newWidth, int newHeight)
{
    // Create OIIO for input and output images.
    int nChannels        = pxr::HioGetComponentCount(imageData.format);
    pxr::HioType hioType = pxr::HioGetHioType(imageData.format);
    OIIO::TypeDesc type  = convertToOIIODataType(hioType);
    OIIO::ImageSpec inSpec(imageData.width, imageData.height, nChannels, type);
    OIIO::ImageSpec outSpec(newWidth, newHeight, nChannels, type);

    // Create buffer for shrunk image.
    size_t compSize = HioGetDataSizeOfType(hioType);
    std::vector<unsigned char> newPixels (newWidth*newHeight*compSize*nChannels);

    // Create image buffers for input and output image.
    OIIO::ImageBuf inBuf(inSpec, imageData.data);
    OIIO::ImageBuf outBuf(outSpec, newPixels.data());

    // Resize the image to new dimensions.
    OIIO::ImageBufAlgo::resize(outBuf, inBuf);

    // Copy the image data and parameters to output structs.
    imageBuf         = newPixels;
    imageData.width  = newWidth;
    imageData.height = newHeight;
    imageData.data   = imageBuf.data();
}

ImageProcessingResolverPlugin::ImageProcessingResolverPlugin() : ArDefaultResolver() {}

ImageProcessingResolverPlugin::~ImageProcessingResolverPlugin() {}

std::shared_ptr<ArAsset> ImageProcessingResolverPlugin::_OpenAsset(
    const ArResolvedPath& resolvedPath) const
{
    // Get string for path.
    std::string pathStr = resolvedPath.GetPathString();

    // Is the path an encoded URI path?
    if (pathStr.find(assetPathPrefix) != std::string::npos)
    {
        // Get the cacheEntry for this URI.
        AssetCacheEntry& cacheEntry =
            const_cast<ImageProcessingResolverPlugin*>(this)->_assetCache[pathStr];

        // If we do not have an asset cached, create one.
        if (!cacheEntry.pAsset)
        {

            // Use the Hio image function to read the source image.
            pxr::HioImageSharedPtr const image =
                pxr::HioImage::OpenForReading(cacheEntry.sourceFilename);

            // If image load failed, return null asset.
            if (!image)
            {
                return nullptr;
            }

            // Create a temp buffer for the source image pixels.
            size_t sizeInBytes = image->GetWidth() * image->GetHeight() * image->GetBytesPerPixel();
            std::vector<unsigned char> tempBuf(sizeInBytes);
            // Read the source image into the temp buffer.
            pxr::HioImage::StorageSpec imageData;
            imageData.width   = image->GetWidth();
            imageData.height  = image->GetHeight();
            imageData.depth   = 1;
            imageData.flipped = false;
            imageData.format  = image->GetFormat();
            imageData.data    = tempBuf.data();
            image->Read(imageData);

            // Get the maxDim query parameter, default to 16k if not specified.
            int maxDim = 16 * 1024;
            cacheEntry.getQuery("maxDim", &maxDim);

            // If the input image's dimensions are larger than maxDim shrink it, keeping the aspect ratio the same.
            if (imageData.width > imageData.height)
            {
                if (imageData.width > maxDim)
                {
                    int newWidth = maxDim;
                    float sizeRatio = float(maxDim) / float(imageData.width);
                    int newHeight   = int(imageData.height * sizeRatio);
                    shrinkImage(imageData, tempBuf, newWidth, newHeight);
                }
            }
            else
            {
                if (imageData.height > maxDim)
                {
                    int newHeight    = maxDim;
                    float sizeRatio = float(maxDim) / float(imageData.height);
                    int newWidth     = int(imageData.width* sizeRatio) ;
                    shrinkImage(imageData, tempBuf, newWidth, newHeight);
                }
            }

            // The asset object (that will be cached in this resolver class.)
            std::shared_ptr<ResolverAsset> pAsset;

            if (cacheEntry.host.compare("linearize") == 0)
            {
                // Run the linearize function if host string indicated that
                // function. This will overwrite the temp buffer in tempBuf with a
                // resized vector if the input is LDR.
                if (!linearize(cacheEntry, image, imageData, tempBuf))
                    return nullptr;
            }
            else if (cacheEntry.host.compare("convertEnvMapLayout") == 0)
            {
                // Run the convert environment map function if host string indicated
                // that function. This will overwrite the temp buffer in tempBuf
                // with a resized vector.
                if (!convertEnvMapLayout(cacheEntry, image, imageData, tempBuf))
                    return nullptr;
            }

            // Get pointer to pixels to be written from temp buffer.
            void* pPixels = tempBuf.data();

            // If the image is in any format except float, then convert it to float.
            pxr::HioType hioType = pxr::HioGetHioType(imageData.format);
            int nChannels        = pxr::HioGetComponentCount(imageData.format);
            std::vector<float> convertedBuf;
            if (hioType != pxr::HioTypeFloat)
            {
                OIIO::TypeDesc type = convertToOIIODataType(hioType);
                OIIO::ImageSpec origImageSpec(imageData.width, imageData.height, nChannels, type);
                OIIO::ImageBuf origBuf(origImageSpec, imageData.data);
                convertedBuf.resize(imageData.width * imageData.height * nChannels * sizeof(float));
                origBuf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::FLOAT, convertedBuf.data());
                imageData.data   = convertedBuf.data();
                imageData.format = convertToFloat(imageData.format);

                // Get the pixels from the converted buffer.
                pPixels = convertedBuf.data();
            }

            // Re-encode the image data as an EXR (as the rest of the USD stack expects an image
            // file from the ArResolver.)
            // The EXR file that is produced only works if OIIO is included in USD. Without OIIO in
            // USD, the calling code will fail to load this.
            // This uses TinyEXR, as a later OIIO version (approx  v2.4.5.0) is required for ioproxy
            // support for EXR or HDR files. This is not ideal as this brings in more dependencies
            // and tinyEXR forces compression, which we don't want.
            //
            unsigned char* pBuffer;
            const char* pErr;
            int len = SaveEXRToMemory((const float*)pPixels, imageData.width, imageData.height,
                nChannels, 0, (const unsigned char**)&pBuffer, &pErr);

            // If successful create an ArAsset from the EXR in memory.
            if (len)
            {
                pAsset = std::make_shared<ResolverAsset>((void*)pBuffer, len);
            }

            // Free the buffer (as the ArAsset has taken a copy.)
            free(pBuffer);

            // Set the cached asset.
            // TODO: Set a memory limit on size of cached assets.
            cacheEntry.pAsset = pAsset;
        }

        // Return the cached asset.
        return cacheEntry.pAsset;
    }

    // If not an imageProcesing URI run the default _OpenAsset Function.
    std::shared_ptr<ArAsset> pRes = ArDefaultResolver::_OpenAsset(resolvedPath);
    return pRes;
}

ArResolvedPath ImageProcessingResolverPlugin::_Resolve(const std::string& assetPath) const
{

    // If this is a processed URI path, generated by _CreateIdentifier, just return
    // it unaltered.
    if (assetPath.find(assetPathPrefix) != std::string::npos)
    {
        return ArResolvedPath(assetPath);
    }

    ArResolvedPath res = ArDefaultResolver::_Resolve(assetPath);
    return res;
}

std::string ImageProcessingResolverPlugin::CreateIdentifierFromURI(
    const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // Parse the asset path as a URI.
    UriUriA uri;
    const char* pErrStr;
    if (uriParseSingleUriA(&uri, assetPath.c_str(), &pErrStr) == URI_SUCCESS)
    {
        // If URI parsing succeeds get the URI scheme.
        std::string scheme(
            uri.scheme.first, (size_t)uri.scheme.afterLast - (size_t)uri.scheme.first);

        // If this is not an imageProcessing URI return empty string.
        if (scheme.compare("imageProcessing") != 0)
            return "";

        // Create an cache entry for this path.
        AssetCacheEntry cacheEntry = AssetCacheEntry();

        // Set the host which defines which processing function to run.
        cacheEntry.host = std::string(
            uri.hostText.first, (size_t)uri.hostText.afterLast - (size_t)uri.hostText.first);

        // Build a query dictionary which defines the function parameters.
        UriQueryListA* queryList;
        int itemCount;
        if (uriDissectQueryMallocA(&queryList, &itemCount, uri.query.first, uri.query.afterLast) ==
            URI_SUCCESS)
        {
            while (queryList)
            {
                cacheEntry.queries[queryList->key] = queryList->value;
                queryList                          = queryList->next;
            }

            // Set the source file name from the file query parameter (Which is
            // common to all functions)
            cacheEntry.sourceFilename = ArDefaultResolver::_CreateIdentifier(
                cacheEntry.queries["filename"], anchorAssetPath);
        }

        // Create a hash from original URI.
        std::size_t assetHash = std::hash<std::string> {}(assetPath.c_str());

        // All images are assigned .exr extension currently.
        string ext = ".exr";

        // Create generated asset path from hash.
        ArResolvedPath generatedAssetPath =
            ArResolvedPath(assetPathPrefix + Aurora::Foundation::sHash(assetHash) + ext);
        cacheEntry.assetPath = generatedAssetPath;

        // If there is no cacheEntry for this path, or the sourceFilename has changed
        // (due to anchor changing) then add to cache.
        auto& assetCache = const_cast<ImageProcessingResolverPlugin*>(this)->_assetCache;
        auto recordIter  = assetCache.find(generatedAssetPath);
        if (recordIter == assetCache.end() ||
            recordIter->second.sourceFilename.compare(cacheEntry.sourceFilename) != 0)
            assetCache[generatedAssetPath] = cacheEntry;

        // Return processed asset path.
        return cacheEntry.assetPath.GetPathString();
    }
    return "";
}

std::string ImageProcessingResolverPlugin::_CreateIdentifier(
    const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // If an imageProcessing URI was successfully parsed, return the processed asset
    // path.
    std::string uriId = CreateIdentifierFromURI(assetPath, anchorAssetPath);
    if (!uriId.empty())
        return uriId;

    // Otherwise return default result.
    return ArDefaultResolver::_CreateIdentifier(assetPath, anchorAssetPath);
}

std::string ImageProcessingResolverPlugin::_CreateIdentifierForNewAsset(
    const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // If an imageProcessing URI was successfully parsed, return the processed asset
    // path.
    std::string uriId = CreateIdentifierFromURI(assetPath, anchorAssetPath);
    if (!uriId.empty())
        return uriId;

    // Otherwise return default result.
    return ArDefaultResolver::_CreateIdentifier(assetPath, anchorAssetPath);
}
