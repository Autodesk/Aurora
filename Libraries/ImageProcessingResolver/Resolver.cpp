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

#include "Resolver.h"
#include "Linearize.h"
#include "ConvertEnvMapLayout.h"

#define URI_STATIC_BUILD 1
#include "uriparser/Uri.h"

#pragma warning(push)
#pragma warning(disable : 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma warning(pop)

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

// Asset path prefix, used once asset URI is processed.
std::string assetPathPrefix = "@@@ImageProcessingAsset_";

ImageProcessingResolverPlugin::ImageProcessingResolverPlugin() : ArDefaultResolver() {}

ImageProcessingResolverPlugin::~ImageProcessingResolverPlugin()
{
}

std::shared_ptr<ArAsset> ImageProcessingResolverPlugin::_OpenAsset(const ArResolvedPath& resolvedPath) const
{
    // Get string for path.
    std::string pathStr = resolvedPath.GetPathString();

    // Is the path an encoded URI path?
    if (pathStr.find(assetPathPrefix) != std::string::npos)
    {
        // Get the cacheEntry for this URI.
        AssetCacheEntry& cacheEntry = const_cast<ImageProcessingResolverPlugin*>(this)->_assetCache[pathStr];

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
            std::vector<unsigned char> imageBuf(sizeInBytes);

            // Read the source image into the temp buffer.
            pxr::HioImage::StorageSpec imageData;
            imageData.width    = image->GetWidth();
            imageData.height   = image->GetHeight();
            imageData.depth    = 1;
            imageData.flipped  = false;
            imageData.format   = image->GetFormat();
            imageData.data = imageBuf.data();
            image->Read(imageData);

            // The asset object (that will be cached in this resolver class.)
            std::shared_ptr<ResolverAsset> pAsset;

            // Currently only RGB float32 supported.
            if (imageData.format == HioFormatFloat32Vec3)
            {
                if (cacheEntry.host.compare("linearize") == 0)
                {
                    // Run the linearize function if host string indicated that function.
                    linearize(cacheEntry, image, imageData);
                }
                else if (cacheEntry.host.compare("convertEnvMapLayout") == 0)
                {
                    // Run the convert environment map function if host string indicated that function.
                    // This will overwrite the temp buffer in imageBuf with a resized vector.
                    convertEnvMapLayout(cacheEntry, image, imageData, imageBuf);
                }

                // Re-encode the image data as an EXR (as the rest of the USD stack expects an image file from the ArResolver.)
                unsigned char* pBuffer;
                const char* pErr;
                int len = SaveEXRToMemory((const float*)imageData.data, imageData.width,
                    imageData.height, 3, 0, (const unsigned char**)&pBuffer, &pErr);

                // If successful create an ArAsset from the EXR in memory.
                if (len)
                {
                    pAsset = std::make_shared<ResolverAsset>((void*)pBuffer, len);
                }

                // Free the buffer (as the ArAsset has taken a copy.)
                free(pBuffer);
            }
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

ArResolvedPath ImageProcessingResolverPlugin::_Resolve(const std::string& assetPath) const {

    // If this is a processed URI path, generated by _CreateIdentifier, just return it unaltered.
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

        // Create a processed asset path string from hash.
        // Assume EXR filename as we only support HDR float32 RGBs currently.
        std::size_t assetHash = std::hash<std::string> {}(assetPath.c_str());
        ArResolvedPath generatedAssetPath =
            ArResolvedPath(assetPathPrefix + Aurora::Foundation::sHash(assetHash) + ".exr");

        // Create an cache entry for this path.
        AssetCacheEntry cacheEntry = AssetCacheEntry(generatedAssetPath);

        // Set the host which defines which processing function to run.
        cacheEntry.host = std::string(
            uri.hostText.first, (size_t)uri.hostText.afterLast - (size_t)uri.hostText.first);

        // Build a query dictionary which defines the function parameters.
        UriQueryListA* queryList;
        int itemCount;
        if (uriDissectQueryMallocA(
                &queryList, &itemCount, uri.query.first, uri.query.afterLast) == URI_SUCCESS)
        {
            while (queryList)
            {
                cacheEntry.queries[queryList->key] = queryList->value;
                queryList                      = queryList->next;
            }

            // Set the source file name from the file query parameter (Which is common to all functions)
            cacheEntry.sourceFilename =
                ArDefaultResolver::_CreateIdentifier(cacheEntry.queries["file"], anchorAssetPath);
        }

        // If there is no cacheEntry for this path, or the sourceFilename has changed (due to anchor changing) then add to cache.
        auto& assetCache = const_cast<ImageProcessingResolverPlugin*>(this)->_assetCache;
        auto recordIter = assetCache.find(generatedAssetPath);
        if (recordIter == assetCache.end() || recordIter->second.sourceFilename.compare(cacheEntry.sourceFilename)!=0)
            assetCache[generatedAssetPath] = cacheEntry;

        // Return processed asset path.
        return cacheEntry.assetPath.GetPathString();

    }
    return "";
}

std::string ImageProcessingResolverPlugin::_CreateIdentifier(
    const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // If an imageProcessing URI was successfully parsed, return the processed asset path.
    std::string uriId = CreateIdentifierFromURI(assetPath, anchorAssetPath);
    if (!uriId.empty())
        return uriId;

    // Otherwise return default result.
    return ArDefaultResolver::_CreateIdentifier(assetPath, anchorAssetPath);
}

std::string ImageProcessingResolverPlugin::_CreateIdentifierForNewAsset(
    const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    // If an imageProcessing URI was successfully parsed, return the processed asset path.
    std::string uriId = CreateIdentifierFromURI(assetPath, anchorAssetPath);
    if (!uriId.empty())
        return uriId;

    // Otherwise return default result.
    return ArDefaultResolver::_CreateIdentifier(assetPath, anchorAssetPath);
}