// Copyright 2022 Autodesk, Inc.
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

struct SceneContents;

// A cache of Aurora images that are loaded from disk as needed.
class ImageCache
{
public:
    // Gets an image from the image cache with the specified file path, or creates a new one if it
    // doesn't exist in the cache.
    // Will linearize from sRGB if linearize parameter is true.
    Aurora::Path getImage(string filePath, Aurora::IScene* pScene, bool linearize)
    {
        // Return empty path if the file path is not specified.
        if (filePath.empty())
        {
            return "";
        }
        Aurora::Path auroraImagePath =
            "PlasmaImage/" + filePath + ":linearize:" + to_string(linearize);

        pScene->setImageFromFilePath(auroraImagePath, filePath, linearize);
        return auroraImagePath;
    }

private:
    using CacheType =
        unordered_map<Aurora::Path, pair<std::unique_ptr<uint8_t[]>, Aurora::ImageDescriptor>>;
    CacheType _cache;
    static uint32_t whitePixels[1];
};

// A type for scene loading function.
using LoadSceneFunc = function<bool(Aurora::IRenderer* pRenderer, Aurora::IScene* pScene,
    const string& filePath, SceneContents& sceneContents)>;

// Loads a Wavefront OBJ (.obj) file into the specified renderer and scene, from the specified file
// path.
bool loadOBJFile(Aurora::IRenderer* pRenderer, Aurora::IScene* pScene, const string& filePath,
    SceneContents& sceneContents);

// Loads a glTF file (.gltf ASCII or .glb binary) into the specified renderer and scene, from the
// specified file path.
bool loadglTFFile(Aurora::IRenderer* pRenderer, Aurora::IScene* pScene, const string& filePath,
    SceneContents& SceneContents);

// Returns the scene loading function for the specified scene file path.
inline LoadSceneFunc getLoadSceneFunc(const string& filePath)
{
    // Get the file extension.
    string extension = filesystem::path { filePath }.extension().string();

    Foundation::sLower(extension);

    // Return the glTF load function for .gltf (ASCII) and .glb (binary).
    if (extension.compare(".gltf") == 0 || extension.compare(".glb") == 0)
    {
        return ::loadglTFFile;
    }

    // Return the OBJ load function for .obj.
    else if (extension.compare(".obj") == 0)
    {
        return ::loadOBJFile;
    }

    // Return no function (unrecognized extension).
    return LoadSceneFunc();
}
