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
#include "pch.h"

#include "Loaders.h"

// Loads a glTF file into the specified renderer and scene, from the specified file path.
bool loadglTFFile(Aurora::IRenderer* /* pRenderer */, Aurora::IScene* /* pScene */,
    const string& filePath, SceneContents& /* sceneContents */)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    string warnings, errors;

    // Attempt to load the file as a binary glTF file, which is (quickly) identified from the file
    // header. If that fails, instead try to load it as an ASCII file.
    bool result = loader.LoadBinaryFromFile(&model, &errors, &warnings, filePath);
    if (!result)
    {
        result = loader.LoadASCIIFromFile(&model, &errors, &errors, filePath);
    }

    // glTF is not yet supported, so for now simply return false.
    // TODO: Load the model into Aurora data structures, similar to the OBJ loader. Also include
    // ".gtlf" and ".glb" in the list of file selection filters.

    return false;
}