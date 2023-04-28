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

#include "HdAuroraInstancer.h"
#include "HdAuroraMaterial.h"
#include "HdAuroraMesh.h"
#include "HdAuroraRenderDelegate.h"
#include "HdAuroraRenderPass.h"
#include "HdAuroraTokens.h"

// If true extra validation done on the HDMesh geometry upon loading.
#define VALIDATE_HDMESH 1

// Vertex data for mesh, passed to Aurora getVertexData callback.  Deleted once vertex update is
// complete.
struct HdAuroraMeshVertexData
{
    VtVec3fArray points;
    VtVec3fArray normals;
    VtVec3fArray tangents;
    VtVec2fArray uvs;
    VtVec3iArray triangulatedIndices;
    VtVec3fArray flattenedPoints;
    VtVec3fArray flattenedNormals;
    VtVec3fArray flattenedTangents;
    VtVec2fArray flattenedUVs;
    const VtArray<GfVec2f>* st;
    bool hasTexCoords;
};

bool HdAuroraMesh::validateIndices(const VtVec3iArray& indices, int maxAttrCount)
{
    uint32_t maxVert = static_cast<uint32_t>(maxAttrCount);
    for (size_t j = 0; j < indices.size(); ++j)
    {
        auto& tri = indices[j];
        // If any of the indices overrun the attribute array completely then skip mesh
        if (static_cast<uint32_t>(tri[0]) >= maxVert || (uint32_t)tri[1] >= maxVert ||
            (uint32_t)tri[2] >= maxVert)
        {
            TF_RUNTIME_ERROR(
                "Invalid index for triangle " + std::to_string(j) + " for " + GetId().GetString());
            return false;
        }
    }
    return true;
}

HdAuroraMesh::HdAuroraMesh(SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate) :
    HdMesh(rprimId), _owner(renderDelegate)
{
}

HdAuroraMesh::~HdAuroraMesh()
{
    _owner->SetSampleRestartNeeded(true);
    ClearAuroraInstances();
}

void HdAuroraMesh::ClearAuroraInstances()
{
    _owner->GetScene()->removeInstances(_auroraInstances);

    _auroraInstances.clear();
}

HdDirtyBits HdAuroraMesh::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllSceneDirtyBits;
}

void HdAuroraMesh::RebuildAuroraInstances(HdSceneDelegate* delegate)
{
    SdfPath id = GetId();

    // Skip adding an Aurora instance if the object is hidden, or there are no indices for it.
    if (!delegate->GetVisible(GetId()))
    {
        if (_instanceData.size())
        {
            // Lock the renderer mutex before accessing Aurora renderer.
            std::lock_guard<std::mutex> rendererLock(_owner->rendererMutex());
            // Remove any existing instances.
            ClearAuroraInstances();
        }

        return;
    }

    // Create vertex data object, that will exist until the renderer has read the vertex data.
    _pVertexData           = make_unique<HdAuroraMeshVertexData>();
    _pVertexData->points   = delegate->Get(id, HdTokens->points).Get<VtVec3fArray>();
    _pVertexData->normals  = delegate->Get(id, HdTokens->normals).Get<VtVec3fArray>();
    _pVertexData->tangents = delegate->Get(id, pxr::TfToken("tangents")).Get<VtVec3fArray>();
    _pVertexData->uvs      = delegate->Get(id, pxr::TfToken("map1")).Get<VtVec2fArray>();

    // Attempt to get UVs using "st" token if "map1" fails.  Both can be used in different circumstances.
    if (_pVertexData->uvs.size()==0)
        _pVertexData->uvs = delegate->Get(id, pxr::TfToken("st")).Get<VtVec2fArray>();

    // Sample code for extracting extra uv set
    // The name for the base uv set is st, other uv sets (st0, st1, etc.)
    HdPrimvarDescriptorVector pvs =
        delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationVertex);

    // Compute the triangulated indices from the mesh topology.
    HdMeshTopology topology = delegate->GetMeshTopology(id);
    HdMeshUtil meshUtil(&topology, id);
    VtIntArray trianglePrimitiveParams;
    meshUtil.ComputeTriangleIndices(&_pVertexData->triangulatedIndices, &trianglePrimitiveParams);

    bool hasFaceVaryingNormals = false;
    bool hasFaceVaryingSTs     = false;
    bool hasFaceVaryings       = false;

    // If we don't have per-vertex UVs look for STs in primvars
    VtValue stVal;
    if (_pVertexData->uvs.size() != _pVertexData->points.size())
    {
        if (readSTs(&stVal, delegate, meshUtil))
        {
            if (stVal.GetArraySize() == _pVertexData->triangulatedIndices.size() * 3)
            {
                hasFaceVaryings   = true;
                hasFaceVaryingSTs = true;
                _pVertexData->st  = &stVal.Get<VtArray<GfVec2f>>();
                if (!_pVertexData->st)
                    TF_WARN("Failed to get STs for " + GetId().GetString());
            }
            else
                TF_WARN("Triangulated ST array length does not match index count for " +
                    GetId().GetString());
        }
        // If failed to get face varyings STs just remove the UVs completely.
        if (!hasFaceVaryingSTs)
            _pVertexData->uvs.clear();
    }

    // Does the mesh have normals?
    bool meshHasNormals = !_pVertexData->normals.empty() &&
        // This is what is received from USD for geometry that has no normals
        !(_pVertexData->normals.size() == 1 && _pVertexData->normals[0][0] == 0.0f &&
            _pVertexData->normals[0][1] == 0.0f && _pVertexData->normals[0][2] == 0.0f);

    if (!meshHasNormals)
    {
        _pVertexData->normals.resize(_pVertexData->points.size());
        Aurora::Foundation::calculateNormals(_pVertexData->points.size(),
            reinterpret_cast<const float*>(_pVertexData->points.data()),
            _pVertexData->triangulatedIndices.size(),
            reinterpret_cast<const unsigned int*>(_pVertexData->triangulatedIndices.data()),
            reinterpret_cast<float*>(_pVertexData->normals.data()));
    }
    // Does the mesh have texture coordinates (UVs or STs)?
    _pVertexData->hasTexCoords = _pVertexData->uvs.size() || _pVertexData->st;

    int minAttrCount = static_cast<int>(_pVertexData->points.size());
    int maxAttrCount = minAttrCount;

    // If there are valid normals but normal and position attribute arrays are not the same
    // clamp indices to smallest value
    VtValue pvNormals;
    if (_pVertexData->normals.size() != _pVertexData->points.size())
    {
        HdPrimvarDescriptorVector fpvs =
            delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationFaceVarying);

        // We should be looking at the material network to see which primvar is use as texcoord, but
        // for now just look for a primvar called "st"
        size_t stIdx;
        for (stIdx = 0; stIdx < fpvs.size(); stIdx++)
        {
            if (fpvs[stIdx].name == HdTokens->normals)
                break;
        }

        if (stIdx != fpvs.size())
        {

            HdPrimvarDescriptor pv = fpvs[stIdx];

            // Get primvar for STs
            auto triNormals = GetPrimvar(delegate, pv.name);
            HdVtBufferSource buffer(pv.name, triNormals);
            int count = (int)buffer.GetNumElements();
            // Triangulate the STs (should produce an ST for each index)
            meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
                buffer.GetData(), count, buffer.GetTupleType().type, &pvNormals);

            if (pvNormals.GetArraySize() == _pVertexData->triangulatedIndices.size() * 3)
            {
                _pVertexData->flattenedNormals = pvNormals.Get<VtVec3fArray>();
                hasFaceVaryingNormals          = true;
                hasFaceVaryings                = true;
            }
            else
                TF_WARN("Triangulated normal array length does not match index count for " +
                    GetId().GetString());
        }
        if (!hasFaceVaryingNormals)
        {
            if (_pVertexData->normals.size() < _pVertexData->points.size())
            {
                TF_WARN("Normal array smaller than points array: " + GetId().GetString());
                while (_pVertexData->normals.size() < _pVertexData->points.size())
                {
                    GfVec3f lv = _pVertexData->normals[_pVertexData->normals.size() - 1];
                    _pVertexData->normals.push_back(lv);
                }
            }
            else
            {
                TF_WARN("Normal array smaller than points array: " + GetId().GetString());
                while (_pVertexData->normals.size() > _pVertexData->points.size())
                {
                    GfVec3f pv = _pVertexData->points[_pVertexData->points.size() - 1];
                    _pVertexData->points.push_back(pv);
                }
            }
        }
    }

    if (hasFaceVaryings)
    {
        size_t numFlattenedVerts = _pVertexData->triangulatedIndices.size() * 3;
        _pVertexData->flattenedPoints.resize(numFlattenedVerts);
        if (!hasFaceVaryingNormals)
            _pVertexData->flattenedNormals.resize(numFlattenedVerts);
        if (_pVertexData->uvs.size() && !hasFaceVaryingSTs)
            _pVertexData->flattenedUVs.resize(numFlattenedVerts);

        // STs are flattened so we must flatten all the attribute data
        // GAM TODO: Could do a lookup here to find matching vertex data and create a new index
        // array (but it will be expensive)
        for (size_t j = 0; j < _pVertexData->triangulatedIndices.size(); ++j)
        {
            auto& tri = _pVertexData->triangulatedIndices[j];
            // If any of the indices overrun the attribute array completely then skip mesh
            if (tri[0] >= maxAttrCount || tri[1] >= maxAttrCount || tri[2] >= maxAttrCount)
            {
                TF_RUNTIME_ERROR("Invalid index for triangle " + std::to_string(j) + " for " +
                    GetId().GetString());
            }

            // Clamp to the minimum attribute count (so something will render without crashing
            // if there is mis-match between point and normal array length)
            int t0 = std::min(minAttrCount - 1, tri[0]);
            int t1 = std::min(minAttrCount - 1, tri[1]);
            int t2 = std::min(minAttrCount - 1, tri[2]);

            _pVertexData->flattenedPoints[j * 3 + 0] = _pVertexData->points[t0];
            _pVertexData->flattenedPoints[j * 3 + 1] = _pVertexData->points[t1];
            _pVertexData->flattenedPoints[j * 3 + 2] = _pVertexData->points[t2];
            if (!hasFaceVaryingNormals)
            {
                _pVertexData->flattenedNormals[j * 3 + 0] = _pVertexData->normals[t0];
                _pVertexData->flattenedNormals[j * 3 + 1] = _pVertexData->normals[t1];
                _pVertexData->flattenedNormals[j * 3 + 2] = _pVertexData->normals[t2];
            };
            if (_pVertexData->uvs.size() && !hasFaceVaryingSTs)
            {
                _pVertexData->flattenedUVs[j * 3 + 0] = _pVertexData->uvs[t0];
                _pVertexData->flattenedUVs[j * 3 + 1] = _pVertexData->uvs[t1];
                _pVertexData->flattenedUVs[j * 3 + 2] = _pVertexData->uvs[t2];
            }
            // If we have tangents provided by client, use them.
            if (_pVertexData->tangents.size() == _pVertexData->normals.size())
            {
                _pVertexData->flattenedTangents[j * 3 + 0] = _pVertexData->tangents[t0];
                _pVertexData->flattenedTangents[j * 3 + 1] = _pVertexData->tangents[t1];
                _pVertexData->flattenedTangents[j * 3 + 2] = _pVertexData->tangents[t2];
            }
        }

        if (_pVertexData->tangents.size() == _pVertexData->normals.size())
        {
            // If there are no client provided tangents, create tangent vectors based on the texture
            // coordinates, using a utility function.
            Aurora::Foundation::calculateTangents(_pVertexData->flattenedPoints.size(),
                reinterpret_cast<const float*>(_pVertexData->flattenedPoints.data()),
                reinterpret_cast<const float*>(_pVertexData->flattenedNormals.data()),
                reinterpret_cast<const float*>(_pVertexData->st->data()),
                _pVertexData->triangulatedIndices.size(), nullptr,
                reinterpret_cast<float*>(_pVertexData->flattenedTangents.data()));
        }
    }
    else
    {
        // If VALIDATE_HDMESH is set do extra validation on the geometry.
        if (VALIDATE_HDMESH && !validateIndices(_pVertexData->triangulatedIndices, maxAttrCount))
            return;

        // Can't handle this case unless we are flattening, so ignore this geometry (will have
        // warned further up)
        if (maxAttrCount != minAttrCount)
            return;

        // Can't handle this case since index is int type.
        if (_pVertexData->points.size() > INT_MAX)
        {
            TF_RUNTIME_ERROR("Invalid vertex count " + std::to_string(_pVertexData->points.size()) +
                " for " + GetId().GetString());
            return;
        }

        if (_pVertexData->tangents.size() != _pVertexData->normals.size() &&
            _pVertexData->uvs.size())
        {
            _pVertexData->tangents.resize(_pVertexData->points.size());
            Aurora::Foundation::calculateTangents(_pVertexData->points.size(),
                reinterpret_cast<const float*>(_pVertexData->points.data()),
                reinterpret_cast<const float*>(_pVertexData->normals.data()),
                reinterpret_cast<const float*>(_pVertexData->uvs.data()),
                _pVertexData->triangulatedIndices.size(),
                reinterpret_cast<const unsigned int*>(_pVertexData->triangulatedIndices.data()),
                reinterpret_cast<float*>(_pVertexData->tangents.data()));
        }
    }

    // Create a geometry descriptor for mesh's geometry.
    Aurora::GeometryDescriptor geomDesc;

    // Fill default vertex attributes.
    geomDesc.vertexDesc.attributes = {
        { Aurora::Names::VertexAttributes::kPosition, Aurora::AttributeFormat::Float3 },
    };

    // Set normals and tangent attibute type.
    geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kNormal] =
        Aurora::AttributeFormat::Float3;
    geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kTangent] =
        Aurora::AttributeFormat::Float3;

    // Set texcoord attibute type, if the mesh has them.
    if (_pVertexData->hasTexCoords)
        geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kTexCoord0] =
            Aurora::AttributeFormat::Float2;

    // Set up index and vertex count.
    int numVertices     = static_cast<int>(_pVertexData->points.size());
    int numIndices      = static_cast<int>(_pVertexData->triangulatedIndices.size() * 3);
    geomDesc.indexCount = _pVertexData->st ? 0ul : numIndices; // No indices if we have STs.
    geomDesc.vertexDesc.count =
        hasFaceVaryings ? numIndices : numVertices; // Vertices are flattened if we have STs.

    // Setup vertex attribute callback to read vertex and index data.
    // This will be called when the geometry is added to scene via instance.
    geomDesc.getAttributeData = [this, hasFaceVaryings, hasFaceVaryingSTs](
                                    Aurora::AttributeDataMap& dataOut, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        // Sanity check, ensure we are not doing a partial update (not actually implemented yet)
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        // Ensure we still have vertex data, if not then this goemetry has been activated,
        // deactivated and then reactivated.
        // TODO: Support this case.
        AU_ASSERT(_pVertexData, "No vertex data for mesh %s, reactivation not supported.",
            GetId().GetString().c_str());

        // We have very different geometry layout if we have STs.
        if (hasFaceVaryings)
        {
            // Sanity check, ensure we are not doing a partial update (not actually implemented yet)
            AU_ASSERT(vertexCount == _pVertexData->flattenedPoints.size(),
                "Partial update not supported");

            // Use the flattened positions and normals.
            dataOut[Aurora::Names::VertexAttributes::kPosition].address =
                &_pVertexData->flattenedPoints[0];
            dataOut[Aurora::Names::VertexAttributes::kPosition].stride = sizeof(GfVec3f);
            dataOut[Aurora::Names::VertexAttributes::kNormal].address =
                &_pVertexData->flattenedNormals[0];
            dataOut[Aurora::Names::VertexAttributes::kNormal].stride = sizeof(GfVec3f);
            dataOut[Aurora::Names::VertexAttributes::kTangent].address =
                &_pVertexData->flattenedTangents[0];
            dataOut[Aurora::Names::VertexAttributes::kTangent].stride = sizeof(GfVec3f);

            // Use the STs.
            if (hasFaceVaryingSTs)
            {
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].address =
                    _pVertexData->st->data();
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].stride = sizeof(GfVec2f);
            }
            else if (_pVertexData->flattenedUVs.size())
            {
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].address =
                    &_pVertexData->flattenedUVs[0];
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].stride = sizeof(GfVec2f);
            }

            // No indices for ST case.
        }
        else
        {
            // Ensure we are not doing a partial update (not actually implemented yet)
            AU_ASSERT(vertexCount == _pVertexData->points.size(), "Partial update not supported");

            // Get position, texcoords, and normals directly from Hydra mesh.
            dataOut[Aurora::Names::VertexAttributes::kPosition].address = &_pVertexData->points[0];
            dataOut[Aurora::Names::VertexAttributes::kPosition].stride  = sizeof(GfVec3f);
            dataOut[Aurora::Names::VertexAttributes::kNormal].address   = &_pVertexData->normals[0];
            dataOut[Aurora::Names::VertexAttributes::kNormal].stride    = sizeof(GfVec3f);
            if (_pVertexData->tangents.size() == _pVertexData->points.size())
            {
                dataOut[Aurora::Names::VertexAttributes::kTangent].address =
                    &_pVertexData->tangents[0];
                dataOut[Aurora::Names::VertexAttributes::kTangent].stride = sizeof(GfVec3f);
            }
            if (_pVertexData->uvs.size())
            {
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].address =
                    &_pVertexData->uvs[0];
                dataOut[Aurora::Names::VertexAttributes::kTexCoord0].stride = sizeof(GfVec2f);
            }

            // Use the triangulated indices computed above.
            AU_ASSERT(firstIndex == 0, "Partial update not supported");
            AU_ASSERT(indexCount == _pVertexData->triangulatedIndices.size() * 3,
                "Partial update not supported");
            dataOut[Aurora::Names::VertexAttributes::kIndices].address =
                &_pVertexData->triangulatedIndices[0];
            dataOut[Aurora::Names::VertexAttributes::kIndices].stride = sizeof(uint32_t);
        }

        return true;
    };

    // Setup completion callback to release data when vertex data has been read.
    // This will be called when the render is done acccesing vertex attributes.
    geomDesc.attributeUpdateComplete = [this](const Aurora::AttributeDataMap&, size_t, size_t,
                                           size_t, size_t) { _pVertexData.reset(); };

    // Actually create the instances in the renderer.
    {
        // Lock the renderer mutex before accessing Aurora renderer.
        std::lock_guard<std::mutex> rendererLock(_owner->rendererMutex());

        // Process material layers.
        // TODO: Some of this can be done outside mutex lock.
        auto materialLayersVal = delegate->Get(GetId(), HdAuroraTokens::kMeshMaterialLayers);
        vector<Aurora::Path> materialLayerPaths;
        vector<Aurora::Path> geometryLayerPaths;
        if (!materialLayersVal.IsEmpty())
        {
            auto geomtryLayerUVsVal = delegate->Get(GetId(), HdAuroraTokens::kMeshGeometryLayerUVs);
            // Get the Hydra ID for material layer.
            auto layerArray = materialLayersVal.Get<pxr::SdfPathVector>();
            _layerUVData    = vector<VtVec2fArray>(layerArray.size());
            for (size_t layerIdx = 0; layerIdx < layerArray.size(); layerIdx++)
            {
                // Convert Hydra material ID to Aurora path,
                auto& auroraMtlPath = HdAuroraMaterial::GetAuroraMaterialPath(layerArray[layerIdx]);
                // Add path to vector.
                materialLayerPaths.push_back(auroraMtlPath);

                // Get the geometry layer UVs.
                Aurora::Path geomLayerPath = "";

                // Value passed to Hydra is primvar name.
                auto uvPrimVarArray = geomtryLayerUVsVal.Get<pxr::VtTokenArray>();
                if (uvPrimVarArray.size() > layerIdx)
                {
                    // UV primvar name.
                    pxr::TfToken primvarName = uvPrimVarArray[layerIdx];

                    // Get the primvar given by name.
                    auto layerUVsVal = delegate->Get(id, primvarName);

                    if (layerUVsVal.IsEmpty())
                    {
                        AU_ERROR("Invalid layer UV%d primvar %s: Primvar not found.", layerIdx,
                            primvarName.GetString().c_str());
                    }
                    else
                    {
                        auto layerUVs = layerUVsVal.Get<VtVec2fArray>();
                        // Create an Aurora geometry object for layer UVs, if any.
                        if (layerUVs.size() != geomDesc.vertexDesc.count)
                        {
                            AU_ERROR(
                                "Invalid layer UV%d primvar %s: Array length %d does not match "
                                "base mesh vertex count of %d",
                                layerIdx, primvarName.GetString().c_str(), layerUVs.size(),
                                geomDesc.vertexDesc.count);
                        }
                        else
                        {
                            // Create path for layer geometry.
                            geomLayerPath =
                                GetId().GetAsString() + "/GeometryLayer" + to_string(layerIdx);
                            _layerUVData[layerIdx] = layerUVs;

                            // Create descriptor for layer.
                            Aurora::GeometryDescriptor layerGeomDesc;

                            // Geometry is incomplete, only containing UVs.
                            layerGeomDesc.vertexDesc.attributes = {
                                { Aurora::Names::VertexAttributes::kTexCoord0,
                                    Aurora::AttributeFormat::Float2 },
                            };
                            layerGeomDesc.indexCount       = 0ul; // No indices.
                            layerGeomDesc.vertexDesc.count = layerUVs.size();

                            // Setup vertex attribute callback to read vertex data.
                            // This will be called when the geometry is added to scene via
                            // instance.
                            layerGeomDesc.getAttributeData =
                                [this, layerIdx](Aurora::AttributeDataMap& dataOut,
                                    size_t /* firstVertex*/, size_t /* vertexCount*/,
                                    size_t /* firstIndex*/, size_t /* indexCount*/) {
                                    // Set the UVs for layer only.
                                    dataOut[Aurora::Names::VertexAttributes::kTexCoord0].address =
                                        &(_layerUVData[layerIdx][0]);
                                    dataOut[Aurora::Names::VertexAttributes::kTexCoord0].stride =
                                        sizeof(GfVec2f);

                                    return true;
                                };

                            // Setup completion callback to release data when UV data has been
                            // read. This will be called when the render is done acccesing
                            // vertex attributes.
                            layerGeomDesc.attributeUpdateComplete =
                                [this, layerIdx](const Aurora::AttributeDataMap&, size_t, size_t,
                                    size_t, size_t) { _layerUVData[layerIdx] = {}; };

                            _owner->GetScene()->setGeometryDescriptor(geomLayerPath, layerGeomDesc);
                        }
                    }
                }

                geometryLayerPaths.push_back(geomLayerPath);
            }
        }
        // Update bounds with this mesh.
        UpdateAuroraSceneBounds(_pVertexData->points);

        // Remove any existing instances.
        ClearAuroraInstances();

        // Update the Aurora material path for the mesh's material.
        UpdateAuroraMaterialPath();

        // Set the material and object ID for all the instances.
        for (auto& instData : _instanceData)
        {
            instData.properties[Aurora::Names::InstanceProperties::kObjectID] = GetPrimId();
            instData.properties[Aurora::Names::InstanceProperties::kMaterial] = _auroraMaterialPath;
            instData.properties[Aurora::Names::InstanceProperties::kMaterialLayers] =
                materialLayerPaths;
            instData.properties[Aurora::Names::InstanceProperties::kGeometryLayers] =
                geometryLayerPaths;
        }

        // Set the geometry descriptor for the instances. Will create it if it doesn't exist.
        Aurora::Path geomPath = id.GetString() + "_Geometry";
        _owner->GetScene()->setGeometryDescriptor(geomPath, geomDesc);

        // Create the instances (this will invoke attribute callbacks).
        _auroraInstances = _owner->GetScene()->addInstances(geomPath, _instanceData);
    }
}

bool isMeshDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points) ||
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals) ||
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::TfToken("st")) ||
        HdChangeTracker::IsTopologyDirty(*dirtyBits, id))
    {
        return true;
    }

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->displayColor) ||
        (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId) ||
        HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
    {
        return true;
    }

    return false;
}

void HdAuroraMesh::UpdateAuroraMaterialPath()
{

    if (!GetMaterialId().IsEmpty())
    {
        // Get material path the Hydra material if it exists.
        _auroraMaterialPath = HdAuroraMaterial::GetAuroraMaterialPath(GetMaterialId());
        if (_owner->GetScene()->getResourceType(_auroraMaterialPath) ==
            Aurora::ResourceType::Invalid)
        {
            // Create a default material if there is no material for this mesh at the .
            _owner->GetScene()->setMaterialType(
                _auroraMaterialPath, Aurora::Names::MaterialTypes::kBuiltIn);
            TF_WARN("No material %s found in scene for mesh %s", _auroraMaterialPath.c_str(),
                GetId().GetString().c_str());
        }
    }
    else
    {
        // Create default Aurora material if we don't have one.
        if (_auroraDefaultMaterialPath.empty())
        {
            // Set base color property on new default material (this will create it as it doesn't
            // yet exist.)
            _auroraDefaultMaterialPath = GetId().GetAsString() + "/DefaultMaterial";
            _owner->GetScene()->setMaterialProperties(
                _auroraDefaultMaterialPath, { { "base_color", _displayColor } });
        }

        // Use default material for this mesh if there is no Hydra material.
        _auroraMaterialPath = _auroraDefaultMaterialPath;
    }
}

bool HdAuroraMesh::readSTs(VtValue* stOut, HdSceneDelegate* delegate, HdMeshUtil& meshUtil)
{

    SdfPath id = GetId();

    HdPrimvarDescriptorVector pvs =
        delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationFaceVarying);

    // We should be looking at the material network to see which primvar is use as texcoord, but for
    // now just look for a primvar called "st"
    size_t stIdx;
    for (stIdx = 0; stIdx < pvs.size(); stIdx++)
    {
        string primVarName = pvs[stIdx].name.GetString();
        if (primVarName.compare("st") == 0 || primVarName.substr(0, 3).compare("map") == 0)
            break;
    }

    if (stIdx == pvs.size())
        return false;

    HdPrimvarDescriptor pv = pvs[stIdx];

    // Get primvar for STs
    auto st = GetPrimvar(delegate, pv.name);
    HdVtBufferSource buffer(pv.name, st);

    // Triangulate the STs (should produce an ST for each index)
    return meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
        buffer.GetData(), (int)buffer.GetNumElements(), buffer.GetTupleType().type, stOut);
}

void HdAuroraMesh::Sync(HdSceneDelegate* delegate, HdRenderParam* /* renderParam */,
    HdDirtyBits* dirtyBits, TfToken const& /* reprToken */)
{
    const auto& id = GetId();
    _owner->SetSampleRestartNeeded(true);

    // Synchronize instancer.
    _UpdateInstancer(delegate, dirtyBits);
    HdInstancer::_SyncInstancerAndParents(delegate->GetRenderIndex(), GetInstancerId());

    // Set the Hydra Mesh object's material ID if it does not match the value from the render
    // delegate.
    bool materialIDChanged = false;
    auto newMatID          = delegate->GetMaterialId(id);
    if (newMatID != GetMaterialId())
    {
        SetMaterialId(newMatID);
        materialIDChanged = true;
    }

    // Calculate dirty flags.
    const bool meshDirty      = isMeshDirty(dirtyBits, id);
    const bool instancesDirty = HdChangeTracker::IsTransformDirty(*dirtyBits, id) ||
        HdChangeTracker::IsInstancerDirty(*dirtyBits, id);
    const bool materialDirty = (*dirtyBits) & HdChangeTracker::RprimDirtyBits::DirtyMaterialId;

    if (_owner->GetRenderer() && (meshDirty || instancesDirty))
    {
        // get mesh transform
        GfMatrix4f meshTM = GfMatrix4f(delegate->GetTransform(id));

        // get instance transforms
        if (!GetInstancerId().IsEmpty())
        {
            // retrieve instance transforms from the instancer.
            VtMatrix4dArray transforms;
            HdInstancer* instancer = delegate->GetRenderIndex().GetInstancer(GetInstancerId());
            transforms =
                static_cast<HdAuroraInstancer*>(instancer)->ComputeInstanceTransforms(GetId());

            // apply instance transforms on top of mesh transforms
            _instanceData.resize(transforms.size());
            for (size_t j = 0; j < transforms.size(); ++j)
            {
                GfMatrix4f xform      = meshTM * GfMatrix4f(transforms[j]);
                _instanceData[j].path = GetId().GetString() + "_Instance" + std::to_string(j);
                _instanceData[j].properties[Aurora::Names::InstanceProperties::kTransform] =
                    GfMatrix4fToGLM(&xform);
            }
        }
        else
        {
            // If there's no instancer, add a single instance with mesh transform.
            _instanceData.resize(1);
            _instanceData[0].path = GetId().GetString() + "_Instance";
            _instanceData[0].properties[Aurora::Names::InstanceProperties::kTransform] =
                GfMatrix4fToGLM(&meshTM);
        }

        // see if we need to add new instances
        const bool addInstances = _auroraInstances.size() > _instanceData.size();

        if (meshDirty || addInstances)
        {
            // Get mesh color (used by default material if no Hydra material for this mesh)
            auto displayColorAttr = delegate->Get(id, HdTokens->displayColor);
            if (displayColorAttr.IsArrayValued())
            {
                VtVec3fArray dispColorArr = displayColorAttr.Get<VtVec3fArray>();
                _displayColor             = GfVec3ToGLM(&dispColorArr[0]);
            }

            // Rebuild the Aurora instances and geometry for this mesh.
            RebuildAuroraInstances(delegate);
        }
        else if (instancesDirty)
        {
            // Lock the renderer mutex before accessing Aurora renderer.
            std::lock_guard<std::mutex> rendererLock(_owner->rendererMutex());

            // size down (if necessary).
            if (_instanceData.size() < _auroraInstances.size())
            {
                Aurora::Paths staleInstances(
                    _auroraInstances.begin() + _instanceData.size(), _auroraInstances.end());
                _owner->GetScene()->removeInstances(staleInstances);
            }
            _auroraInstances.resize(_instanceData.size());

            // update transforms only (as other properties have not changed)
            for (size_t i = 0; i < _instanceData.size(); ++i)
            {
                _owner->GetScene()->setInstanceProperties(_auroraInstances[i],
                    { { Aurora::Names::InstanceProperties::kTransform,
                        _instanceData[i].properties.at(
                            Aurora::Names::InstanceProperties::kTransform) } });
            }

            // Update bounds with this mesh.
            UpdateAuroraSceneBounds(delegate->Get(id, HdTokens->points).Get<VtVec3fArray>());
        }
        else if (materialDirty || materialIDChanged)
        {
            // Lock the renderer mutex before accessing Aurora renderer.
            std::lock_guard<std::mutex> rendererLock(_owner->rendererMutex());

            // Update the aurora material paths.
            UpdateAuroraMaterialPath();

            // Set the material for all the instances.
            _owner->GetScene()->setInstanceProperties(_auroraInstances,
                { { Aurora::Names::InstanceProperties::kMaterial, _auroraMaterialPath } });
        }
    }
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void HdAuroraMesh::UpdateAuroraSceneBounds(const VtVec3fArray& points)
{
    // Calculate mesh bounds in local space.
    GfVec3f minBounds(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    GfVec3f maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (size_t i = 0; i < points.size(); i++)
    {
        const GfVec3f& pnt = points[i];
        for (int j = 0; j < 3; j++)
        {
            minBounds[j] = std::min(minBounds[j], pnt[j]);
            maxBounds[j] = std::max(maxBounds[j], pnt[j]);
        }
    }

    // Construct array of 8 bound vertices
    GfVec3f boundPoints[8] = { { minBounds[0], minBounds[1], minBounds[2] },
        { minBounds[0], maxBounds[1], minBounds[2] }, { minBounds[0], minBounds[1], maxBounds[2] },
        { minBounds[0], maxBounds[1], maxBounds[2] }, { maxBounds[0], minBounds[1], minBounds[2] },
        { maxBounds[0], maxBounds[1], minBounds[2] }, { maxBounds[0], minBounds[1], maxBounds[2] },
        { maxBounds[0], maxBounds[1], maxBounds[2] } };

    // Merge in world space bounds of every instance
    minBounds = GfVec3f(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    maxBounds = GfVec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& instanceData : _instanceData)
    {
        // Get the instance's transform.
        glm::mat4 transformGlm =
            instanceData.properties.at(Aurora::Names::InstanceProperties::kTransform).asMatrix4();

        // Convert to GF matrix.
        GfMatrix4f transform = GLMMat4ToGF(&transformGlm);

        // Transform corners of AABB
        for (int i = 0; i < 8; i++)
        {
            const GfVec3f& pnt = transform.Transform(boundPoints[i]);
            for (int j = 0; j < 3; j++)
            {
                minBounds[j] = std::min(minBounds[j], pnt[j]);
                maxBounds[j] = std::max(maxBounds[j], pnt[j]);
            }
        }
    }

    // Ensure bounds are valid.
    for (int j = 0; j < 3; j++)
    {
        float diff = maxBounds[j] - minBounds[j];
        if (diff == 0.0f)
        {
            maxBounds[j] = minBounds[j] + FLT_EPSILON;
        }
    }

    // Update owner bounds
    _owner->UpdateBounds(minBounds, maxBounds);
}

HdDirtyBits HdAuroraMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void HdAuroraMesh::_InitRepr(TfToken const& reprToken, HdDirtyBits*)
{
    _ReprVector::iterator it =
        std::find_if(_reprs.begin(), _reprs.end(), _ReprComparator(reprToken));
    if (it == _reprs.end())
    {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

void HdAuroraMesh::Finalize(HdRenderParam* /* renderParam */) {}
