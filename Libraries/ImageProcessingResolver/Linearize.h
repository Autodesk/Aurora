#include "Resolver.h"
#include <pxr/imaging/hio/image.h>

extern bool linearize(const AssetCacheEntry& record, pxr::HioImageSharedPtr const pImage,
    pxr::HioImage::StorageSpec& imageData);
