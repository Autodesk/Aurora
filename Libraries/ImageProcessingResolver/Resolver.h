#pragma once

#include <pxr/usd/ar/defaultResolver.h>
#include <pxr/usd/ar/asset.h>

#include <tbb/enumerable_thread_specific.h>

#include <memory>
#include <string>
#include <vector>
#include <map>


class ResolverAsset;

struct AssetCacheEntry
{
    AssetCacheEntry(pxr::ArResolvedPath path = pxr::ArResolvedPath()) : assetPath(path) {}

    // Pointer to asset for this cache entry.
    std::shared_ptr<ResolverAsset> pAsset;
    // Scheme string from this entry's URI.
    std::string scheme;
    // Host string from this entry's URI.
    std::string host;
    // Dictionary of query strings from this entry's URI.
    std::map<std::string, std::string> queries;
    // Source filename for this entry (after anchor path applied.)
    std::string sourceFilename;
    // Processed asset path.
    pxr::ArResolvedPath assetPath;

    // Get query from dictionary as float.
    bool getQuery(const std::string key, float* pValOut) const
    {
        std::string strVal;

        if (!getQuery(key, &strVal))
            return false;

        *pValOut = (float)std::atof(strVal.c_str());
        return true;
    }

    // Get query from dictionary as string.
    bool getQuery(const std::string key, std::string* pValOut) const
    {
        auto iter = queries.find(key);
        if (iter == queries.end())
            return false;

        *pValOut = iter->second;
        return true;
    }
};

// Custom asset class for ImageProcessingResolverPlugin
class ResolverAsset : public pxr::ArAsset
{
public:
    // Ctor will copy contents of pData to newly allocated asset buffer.
    ResolverAsset(void *pData, size_t size) : _pData(new char[size], std::default_delete<char[]>())
    {
        memcpy(_pData.get(), pData, size);
        _size  = size;
    }

    // Get size of asset in bytes.
    virtual size_t GetSize() const override { return _size; }

    // Get const pointer to asset buffer.
    virtual std::shared_ptr<const char> GetBuffer() const override { return _pData; }
    // Get pointer to asset buffer.
    std::shared_ptr<char> GetBuffer() { return _pData; }

    // Read bytes from asset buffer.
    virtual size_t Read(void* buffer, size_t count, size_t offset) const override
    { 
        
        if (offset+count>_size)
        {
            if (offset >= _size)
                return 0;
            count = _size-offset;
        }
        memcpy(buffer, _pData.get() + offset, count);

        return count;
    }

    // Not implemented.
    virtual std::pair<FILE*, size_t> GetFileUnsafe() const override {
        return std::make_pair<FILE*, size_t>(nullptr, 0);
    }

protected:
    std::shared_ptr<char> _pData;
    size_t _size;
};

// ArResolver plugin for image processing.
class ImageProcessingResolverPlugin : public pxr::ArDefaultResolver
{
public:
    ImageProcessingResolverPlugin();
    ~ImageProcessingResolverPlugin() override;

    // Overriden ArResolver::_OpenAsset implementation.
    std::shared_ptr<pxr::ArAsset> _OpenAsset(
        const pxr::ArResolvedPath& resolvedPath) const override;
    // Overriden ArResolver::_Resolve implementation.
    pxr::ArResolvedPath _Resolve(const std::string& assetPath) const override;

protected:
    // Overriden ArResolver::_CreateIdentifier implementation.
    std::string _CreateIdentifier(
        const std::string& assetPath, const pxr::ArResolvedPath& anchorAssetPath) const override;
    // Overriden ArResolver::_CreateIdentifierForNewAsset implementation.

    // Utility function to create identifier from URI, and add to cache.
    std::string _CreateIdentifierForNewAsset(
        const std::string& assetPath, const pxr::ArResolvedPath& anchorAssetPath) const override;


    std::string CreateIdentifierFromURI(
        const std::string& assetPath, const pxr::ArResolvedPath& anchorAssetPath) const;

    std::map<std::string, AssetCacheEntry> _assetCache;
};

