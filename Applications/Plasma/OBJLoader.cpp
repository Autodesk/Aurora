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

#include "Aurora/Foundation/Geometry.h"
#include "Loaders.h"
#include "SceneContents.h"

uint32_t ImageCache::whitePixels[1] = { 0xFFFFFFFF };

#define PLASMA_HAS_TANGENTS 0

// A simple structure describing an OBJ index, which has sub-indices for vertex channels: position,
// normal, and texture coordinates.
struct OBJIndex
{
    // Constructor, from a tinyobj::index_t.
    OBJIndex(const tinyobj::index_t& index) :
        Position(index.vertex_index), Normal(index.normal_index), TexCoord(index.texcoord_index)
    {
    }

    // Equality operator, for use in a hash map.
    bool operator==(const OBJIndex& other) const
    {
        return Position == other.Position && Normal == other.Normal && TexCoord == other.TexCoord;
    }

    // Indices, same as tinyobj::index_t.
    int Position;
    int Normal;
    int TexCoord;
};

// A hash function object for OBJIndex.
struct hashOBJIndex
{
    size_t operator()(const OBJIndex& index) const
    {
        // Get hash values for the individual indices...
        hash<int> hasher;
        size_t h1 = hasher(index.Position);
        size_t h2 = hasher(index.Normal);
        size_t h3 = hasher(index.TexCoord);

        // ... and combine them in one recommended way.
        return ((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1);
    }
};

// An unordered (hash) map from an OBJIndex (with vertex channel indices) to a single index.
using OBJIndexMap = unordered_map<OBJIndex, uint32_t, hashOBJIndex>;

// Loads a Wavefront OBJ file into the specified renderer and scene, from the specified file path.
bool loadOBJFile(Aurora::IRenderer* /*pRenderer*/, Aurora::IScene* pScene, const string& filePath,
    SceneContents& sceneContents)
{
    sceneContents.reset();

    // Start a timer for reading the OBJ file.
    Foundation::CPUTimer timer;
    ::infoMessage("Reading OBJ file \"" + filePath + "\"...");

    // Load the OBJ file with tinyobjloader. If the load is not successful or has no shapes, do
    // nothing else.
    // NOTE: tinyobjloader currently does not support wide strings on Windows, so files with wide
    // non-ASCII characters in their names will fail to load.
    tinyobj::attrib_t attrib;
    vector<tinyobj::shape_t> shapes;
    vector<tinyobj::material_t> objMaterials;
    string sWarnings;
    string sErrors;
    uint32_t objectCount = 0;
    bool result =
        tinyobj::LoadObj(&attrib, &shapes, &objMaterials, &sWarnings, &sErrors, filePath.c_str());
    if (!result || shapes.empty())
    {
        return false;
    }

    // Report the file read time.
    ::infoMessage("... completed in " + to_string(static_cast<int>(timer.elapsed())) + " ms.");

    // Start a timer for translating the OBJ scene data.
    timer.reset();
    ::infoMessage("Translating OBJ scene data...");

    // Parse the OBJ materials into a corresponding array of Aurora materials.
    vector<Aurora::Path> lstMaterials;
    lstMaterials.reserve(objMaterials.size());
    ImageCache imageCache;
    int mtlCount = 0;
    for (auto& objMaterial : objMaterials)
    {
        // Collect material properties.
        // NOTE: This includes support for some of the properties in the PBR extension here:
        // http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
        // > We use the Standard Surface IOR default of 1.5 when no IOR is specified in the file,
        //   which is parsed as 1.0.
        // > Transmission is derived from the "dissolve" property. This is normally intended for
        //   what Standard Surface calls "opacity" but transmission is more interesting.
        // > The default transmittance is black, which is a useless tint color, so that value is
        //   converted to white.
        vec3 baseColor          = ::sRGBToLinear(make_vec3(objMaterial.diffuse));
        float metalness         = objMaterial.metallic;
        vec3 specularColor      = ::sRGBToLinear(make_vec3(objMaterial.specular));
        float specularRoughness = objMaterial.roughness;
        float specularIOR       = objMaterial.ior == 1.0f ? 1.5f : objMaterial.ior;
        float transmission      = 1.0f - objMaterial.dissolve;
        vec3 transmissionColor  = ::sRGBToLinear(make_vec3(objMaterial.transmittance));
        if (length(transmissionColor) == 0.0f)
        {
            transmissionColor = vec3(1.0f);
        }
        vec3 opacity = vec3(1.0f); // objMaterial.dissolve (see NOTE above)

        // Load the base color image file from the "map_Kd" file path.
        // NOTE: Set the linearize flag, as these images typically use the sRGB color space.
        Aurora::Path baseColorImage =
            imageCache.getImage(objMaterial.diffuse_texname, pScene, true);

        // Load the specular roughness image file from the "map_Pr" file path.
        // NOTE: Don't set the linearize flag, as these images typically use the linear color space.
        Aurora::Path specularRoughnessImage =
            imageCache.getImage(objMaterial.roughness_texname, pScene, false);

        // Load the opacity image file from the "map_d" file path.
        // NOTE: Don't set the linearize flag, as these images typically use the linear color space.
        Aurora::Path opacityImage = imageCache.getImage(objMaterial.alpha_texname, pScene, false);

        // Load the normal image file from either the "bump" or "norm" file path in the material.
        // The file could be specified in either property depending on the exporter.
        // NOTE: Don't set the linearize flag, as these images typically use the linear color space.
        // Only normal maps (not height maps) are supported here.
        string normalFilePath    = objMaterial.normal_texname.empty() ? objMaterial.bump_texname
                                                                      : objMaterial.normal_texname;
        Aurora::Path normalImage = imageCache.getImage(normalFilePath, pScene, false);

        Aurora::Path materialPath = filePath + ":OBJFileMaterial-" + to_string(mtlCount++);

        // Create an Aurora material, assign the properties, and add the material to the list.
        pScene->setMaterialProperties(materialPath,
            { { "base_color", (baseColor) }, { "metalness", metalness },
                { "specular_color", (specularColor) }, { "specular_roughness", specularRoughness },
                { "specular_IOR", specularIOR }, { "transmission", transmission },
                { "transmission_color", (transmissionColor) }, { "opacity", (opacity) },
                { "base_color_image", baseColorImage },
                { "specular_roughness_image", specularRoughnessImage },
                { "opacity_image", opacityImage }, { "normal_image", normalImage } });
        lstMaterials.push_back(materialPath);
    };

    // Get the data arrays from the tinyobj::attrib_t object.
    const auto& srcPositions = attrib.vertices;
    const auto& srcNormals   = attrib.normals;
    const auto& srcTexCoords = attrib.texcoords;

    // Iterate the shapes, creating an Aurora geometry for each one and adding an instance of it
    // to the scene.
    bool hasMesh = false;

    for (const auto& shape : shapes)
    {

        // Skip shapes that don't have a mesh (e.g. lines or points).
        auto indexCount = static_cast<uint32_t>(shape.mesh.indices.size());
        if (indexCount == 0)
        {
            continue;
        }
        hasMesh = true;

        Aurora::Path sceneInstancePath =
            filePath + "-" + shape.name + ":OBJFileInstance-" + to_string(objectCount);
        Aurora::Path geomPath =
            filePath + "-" + shape.name + ":OBJFileGeom-" + to_string(objectCount);
        objectCount++;

        SceneGeometryData& geometryData = sceneContents.addGeometry(geomPath);
        auto& positions                 = geometryData.positions;
        auto& normals                   = geometryData.normals;
        auto& tex_coords                = geometryData.texCoords;
        auto& indices                   = geometryData.indices;
        indices.reserve(indexCount);

        bool bHasNormals   = shape.mesh.indices[0].normal_index >= 0;
        bool bHasTexCoords = shape.mesh.indices[0].texcoord_index >= 0;

        // Iterate the "OBJ indices" in the shape, which consist of three sub-indices for individual
        // position, normal, and texture coordinate values. These are used to define unique vertices
        // (i.e. identical combinations of the three sub-indices) and populate the data arrays.
        uint32_t nextIndex = 0;
        OBJIndexMap objIndexMap;
        for (const auto& objIndex : shape.mesh.indices)
        {
            // If this OBJ index is already in the map, add the corresponding unique index to the
            // index array. The OBJIndex struct supports hashing to allow this.
            if (objIndexMap.find(objIndex) != objIndexMap.end())
            {
                indices.push_back(objIndexMap[objIndex]);
            }

            // Else: Add the corresponding position, normal, and texture coordinate values to the
            // their respective arrays. Also add the next index value to the index array and the OBJ
            // index map.
            else
            {
                // Add the position (XYZ) value, including updating the bounding box if needed.
                const float* position = &srcPositions[objIndex.vertex_index * 3];
                sceneContents.bounds.add(position);
                positions.push_back(*position++);
                positions.push_back(*position++);
                positions.push_back(*position);

                // Add the normal (XYZ) value, if needed.
                if (bHasNormals)
                {
                    // Normalize the normal, in case the source data has bad normals.
                    glm::vec3 normal =
                        glm::normalize(make_vec3(&srcNormals[objIndex.normal_index * 3]));

                    // Add the normal.
                    normals.push_back(normal.x);
                    normals.push_back(normal.y);
                    normals.push_back(normal.z);
                }

                // Add the texture coordinate (UV) value, if needed.
                if (bHasTexCoords)
                {
                    const float* tex_coord = &srcTexCoords[objIndex.texcoord_index * 2];
                    tex_coords.push_back(*tex_coord++);
                    tex_coords.push_back(1.0f - *tex_coord); // flip V
                }

                // Add the next index to the index array and the OBJ index map, for the current OBJ
                // index.
                indices.push_back(nextIndex);
                objIndexMap[objIndex] = nextIndex;
                nextIndex++;
            }
        }

        // Calculate the vertex count.
        uint32_t vertexCount = static_cast<uint32_t>(positions.size()) / 3;

        // Do we have tangents ? Default to false.
        bool bHasTangents = false;

        // Create normals if they are not available.
        if (!bHasNormals)
        {
            normals.resize(positions.size());
            Foundation::calculateNormals(
                vertexCount, positions.data(), indexCount / 3, indices.data(), normals.data());
        }

        // Create tangents if texture coordinates are available.
#if PLASMA_HAS_TANGENTS
        auto& tangents = geometryData.tangents;
        if (bHasTexCoords)
        {
            tangents.resize(normals.size());
            Foundation::calculateTangents(vertexCount, positions.data(), normals.data(),
                tex_coords.data(), indexCount / 3, indices.data(), tangents.data());
            bHasTangents = true;
        }
#endif

        Aurora::GeometryDescriptor& geomDesc = geometryData.descriptor;
        geomDesc.type                        = Aurora::PrimitiveType::Triangles;
        geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kPosition] =
            Aurora::AttributeFormat::Float3;
        geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kNormal] =
            Aurora::AttributeFormat::Float3;
        geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kTexCoord0] =
            Aurora::AttributeFormat::Float2;
        if (bHasTangents)
            geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kTangent] =
                Aurora::AttributeFormat::Float3;
        geomDesc.vertexDesc.count = vertexCount;
        geomDesc.indexCount       = indexCount;
        geomDesc.getAttributeData = [geomPath, bHasTangents, &sceneContents](
                                        Aurora::AttributeDataMap& buffers, size_t /* firstVertex*/,
                                        size_t /* vertexCount*/, size_t /* firstIndex*/,
                                        size_t /* indexCount*/) {
            SceneGeometryData& geometryData = sceneContents.geometry[geomPath];

            buffers[Aurora::Names::VertexAttributes::kPosition].address =
                geometryData.positions.data();
            buffers[Aurora::Names::VertexAttributes::kPosition].size =
                geometryData.positions.size() * sizeof(float);
            buffers[Aurora::Names::VertexAttributes::kPosition].stride = sizeof(vec3);
            buffers[Aurora::Names::VertexAttributes::kNormal].address = geometryData.normals.data();
            buffers[Aurora::Names::VertexAttributes::kNormal].size =
                geometryData.normals.size() * sizeof(float);
            buffers[Aurora::Names::VertexAttributes::kNormal].stride = sizeof(vec3);
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].address =
                geometryData.texCoords.data();
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].size =
                geometryData.texCoords.size() * sizeof(float);
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

            // Fill in the tangent data, if we have them.
            if (bHasTangents)
            {
                buffers[Aurora::Names::VertexAttributes::kTangent].address =
                    geometryData.tangents.data();
                buffers[Aurora::Names::VertexAttributes::kTangent].size =
                    geometryData.tangents.size() * sizeof(float);
                buffers[Aurora::Names::VertexAttributes::kTangent].stride = sizeof(vec3);
            }

            buffers[Aurora::Names::VertexAttributes::kIndices].address =
                geometryData.indices.data();
            buffers[Aurora::Names::VertexAttributes::kIndices].size =
                geometryData.indices.size() * sizeof(int);
            buffers[Aurora::Names::VertexAttributes::kIndices].stride = sizeof(int);

            return true;
        };

        pScene->setGeometryDescriptor(geomPath, geomDesc);

        // Create an Aurora geometry object and get the associated material. Add an instance with
        // that geometry and material to the scene.
        // NOTE: An OBJ file can have different materials for each face (triangle) in a mesh. Here
        // we only use the material assigned to the *first* face for the entire mesh. If no material
        // is specified, use a null pointer which indicates Aurora should use a default material.
        int material_id = shape.mesh.material_ids[0];

        // Add instance to the scene.
        Aurora::InstanceDefinition instDef = { sceneInstancePath,
            { { Aurora::Names::InstanceProperties::kTransform, mat4() } } };

        if (material_id >= 0)
            instDef.properties[Aurora::Names::InstanceProperties::kMaterial] =
                lstMaterials[material_id];
        pScene->addInstance(sceneInstancePath, geomPath, instDef.properties);

        sceneContents.instances.push_back({ instDef, geomPath });
        sceneContents.vertexCount += static_cast<uint32_t>(geomDesc.vertexDesc.count);
        sceneContents.triangleCount += static_cast<uint32_t>(geomDesc.indexCount / 3);
    }

    // Report the translation time.
    ::infoMessage("... completed in " + to_string(static_cast<int>(timer.elapsed())) + " ms.");

    return hasMesh;
}
