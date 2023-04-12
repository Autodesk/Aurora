#include "Resolver.h"
#include <pxr/imaging/hio/image.h>

extern bool convertEnvMapLayout(const AssetCacheEntry& cacheEntry, pxr::HioImageSharedPtr const pImage,
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf);

