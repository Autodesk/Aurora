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

#include "ConvertEnvMapLayout.h"
#include "Resolver.h"
#include <Aurora/Foundation/Log.h>
#include <Aurora/Foundation/Utilities.h>

int gcd(int a, int b)
{
    return b ? gcd(b, a % b) : a;
}

std::vector<OIIO::ImageBuf> getSixFacesFromSourceImage(pxr::HioImage::StorageSpec& imageData)
{
    int numChannels                          = pxr::HioGetComponentCount(imageData.format);
    pxr::HioType hioType                     = pxr::HioGetHioType(imageData.format);
    int iGcd                                 = gcd(imageData.width, imageData.height);
    int iRatio                               = 0;
    iRatio                                   = imageData.width / iGcd;
    std::vector<OIIO::ImageBuf> vecImageList = {};
    OIIO::TypeDesc type                      = convertToOIIODataType(hioType);
    OIIO::ImageSpec subImageSpec(iGcd, iGcd, numChannels, type);
    OIIO::ImageSpec srcImageSpec(imageData.width, imageData.height, numChannels, type);
    OIIO::ImageBuf srcBuf(srcImageSpec, imageData.data);
    OIIO::ImageBuf des(subImageSpec);
    std::stringstream s;
    switch (iRatio)
    {
        // Vertical cubemap, subimages left to right +X,-X,+Y,-Y,+Z,-Z
    case 1:
        for (int i = 0; i < 6; ++i)
        {
            OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { 0, iGcd, i * iGcd, (i + 1) * iGcd });
            vecImageList.push_back(des);
        }
        break;
    case 3:
        OIIO::ImageBufAlgo::cut(
            des, srcBuf, OIIO::ROI { 0, iGcd + 1, iGcd, 2 * iGcd }); //-X  //roi [begin,end)
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(
            des, srcBuf, OIIO::ROI { 2 * iGcd - 1, 3 * iGcd, iGcd, 2 * iGcd }); //+X
        des = OIIO::ImageBufAlgo::flip(des);
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, 2 * iGcd, 3 * iGcd }); //-Y
        des = OIIO::ImageBufAlgo::flip(des);
        des = OIIO::ImageBufAlgo::flop(des);
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, 0, iGcd }); //+Y
        des = OIIO::ImageBufAlgo::flip(des);
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, 3 * iGcd, 4 * iGcd }); //+Z
        des = OIIO::ImageBufAlgo::flip(des);
        des = OIIO::ImageBufAlgo::flop(des);
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, iGcd, 2 * iGcd }); //-Z
        vecImageList.push_back(des);
        break;
    case 4:
        OIIO::ImageBufAlgo::cut(
            des, srcBuf, OIIO::ROI { 2 * iGcd - 1, 3 * iGcd, iGcd, 2 * iGcd }); //+X
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { 0, iGcd + 1, iGcd, 2 * iGcd }); //-X
        des = OIIO::ImageBufAlgo::flip(des);
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, 2 * iGcd, 3 * iGcd }); //+Y
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, 0, iGcd }); //-Y
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { iGcd, 2 * iGcd, iGcd, 2 * iGcd }); //+Z
        vecImageList.push_back(des);
        OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { 3 * iGcd, 4 * iGcd, iGcd, 2 * iGcd }); //-Z
        vecImageList.push_back(des);
        break;
    case 6:
        // Horizital cubemap, subimages from top to bottom like +X,-X,+Y,-Y,+Z,-Z
        for (int i = 0; i < 6; ++i)
        {
            OIIO::ImageBufAlgo::cut(des, srcBuf, OIIO::ROI { i * iGcd, (i + 1) * iGcd, 0, iGcd });
            vecImageList.push_back(des);
        }
        break;
    default:
        break;
    }
    return vecImageList;
}

bool convertToLatLongLayout(
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf)
{
    int nChannels        = pxr::HioGetComponentCount(imageData.format);
    pxr::HioType hioType = pxr::HioGetHioType(imageData.format);
    OIIO::TypeDesc type  = convertToOIIODataType(hioType);
    OIIO::ImageSpec spec(imageData.width, imageData.height, nChannels, type);

    spec.full_x       = spec.x;
    spec.full_y       = spec.y;
    spec.full_z       = spec.z;
    spec.full_width   = spec.width;
    spec.full_height  = spec.height;
    spec.full_depth   = spec.depth;
    int patch         = gcd(imageData.width, imageData.height);
    size_t outputSize = 4 * patch * 2 * patch * nChannels * sizeof(float);
    OIIO::ImageSpec latlongspec(4 * patch, 2 * patch, nChannels, type);
    std::vector<unsigned char> newPixels(outputSize);
    unsigned char* outputImageBuffer = newPixels.data();
    OIIO::ImageBuf latlong(latlongspec, outputImageBuffer);
    float pixelGotten[4];
    // Intergrate a latlong layout image from hcross/vcross
    // 1.Construct the circumscribed sphere of the cube
    // 2.Fill the sphere patch with cube surface( six images)
    // 3.Expand the sphere along the 0 degree longitude
    std::vector<OIIO::ImageBuf> sixSidesList = {};
    sixSidesList                             = getSixFacesFromSourceImage(imageData);
    for (int h = 0; h < 2 * patch; ++h)
    {
        float const phi = static_cast<float>(M_PI * (h - patch) / (2 * patch)); //-pi/2~pi/2
        for (int w = 0; w < 4 * patch; ++w)
        {
            int coord_x, coord_y;
            // Translate from sphere coordinate to Cartesian coordinate.
            float theta = static_cast<float>(M_PI * (w - 2 * patch) / (2 * patch)); // -pi~pi
            float x     = cos(phi) * sin(theta);
            float y     = sin(phi);
            float z     = cos(phi) * cos(theta);
            if (abs(x) >= abs(y) && abs(x) >= abs(z))
            {
                coord_x = static_cast<int>(patch * (1.0 - z / x) / 2.0);
                coord_y = static_cast<int>(patch * (1.0 + y / x) / 2.0);
                x < 0.0 ? sixSidesList.at(1).getpixel(coord_x, coord_y, pixelGotten)
                        : sixSidesList.at(0).getpixel(coord_x, coord_y, pixelGotten);
            }
            else if (abs(y) > abs(z))
            {
                coord_x = static_cast<int>(patch * (1 + x / y) / 2.0);
                coord_y = static_cast<int>(patch * (1 - z / y) / 2.0);
                y < 0.0 ? sixSidesList.at(3).getpixel(coord_x, coord_y, pixelGotten)
                        : sixSidesList.at(2).getpixel(coord_x, coord_y, pixelGotten);
            }
            else
            {
                coord_x = static_cast<int>(patch * (1 + x / z) / 2.0);
                coord_y = static_cast<int>(patch * (1 + z / abs(z) * y / z) / 2.0);
                z < 0.0 ? sixSidesList.at(5).getpixel(coord_x, coord_y, pixelGotten)
                        : sixSidesList.at(4).getpixel(coord_x, coord_y, pixelGotten);
            }
            latlong.setpixel(w, h, pixelGotten);
        }
    }
    imageBuf         = newPixels;
    imageData.width  = 4 * patch;
    imageData.height = 2 * patch;
    imageData.data   = imageBuf.data();
    return true;
}

bool convertEnvMapLayout(const AssetCacheEntry& record, pxr::HioImageSharedPtr const /*pImage*/,
    pxr::HioImage::StorageSpec& imageData, std::vector<unsigned char>& imageBuf)
{

    std::string targetLayout;
    record.getQuery("targetLayout", &targetLayout);
    if (targetLayout.compare("latlon") != 0)
    {
        AU_WARN("Only latlon target layout supported.");
        return false;
    }

    // If the width is 2x the height then we infer the image is already in lat-long layout.
    if (imageData.width == imageData.height * 2)
    {
        AU_WARN(
            "Image dimensions imply image is already in lat-long format, returning unconverted "
            "image.");
        return true;
    }

    std::string sourceLayout;
    record.getQuery("sourceLayout", &sourceLayout);
    if (sourceLayout.compare("cube") != 0)
    {
        AU_WARN("Only cube source layout supported.");
        return false;
    }

    convertToLatLongLayout(imageData, imageBuf);
    return true;
}
