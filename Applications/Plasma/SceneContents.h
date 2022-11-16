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

// A structure used to keep a copy of all the vertex and index data used to create instance.
struct SceneGeometryData
{
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texCoords;

    std::vector<std::uint32_t> indices;
    Aurora::GeometryDescriptor descriptor;
};

struct SceneInstanceData
{
    Aurora::InstanceDefinition def;
    Aurora::Path geometryPath;
};

// Contents of a loaded scene.
struct SceneContents
{
    // Description of instances in the scene.
    std::vector<SceneInstanceData> instances;

    std::map<Aurora::Path, SceneGeometryData> geometry;

    SceneGeometryData& addGeometry(Aurora::Path path)
    {
        geometry[path] = SceneGeometryData();
        return geometry[path];
    }

    // Number of vertices in the loaded scene.
    uint32_t vertexCount = 0;

    // Number of triangles in the loaded scene.
    uint32_t triangleCount = 0;

    // The world space bounds of the loaded scene.
    Foundation::BoundingBox bounds;

    // Clear the contents all the loaded values instance data.
    void reset();
};