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

#pragma once

class HdAuroraMaterial;
class HdAuroraRenderDelegate;
struct HdAuroraMeshVertexData;
class HdAuroraMesh : public HdMesh
{
public:
    HdAuroraMesh(SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraMesh() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
        TfToken const& reprToken) override;
    void Finalize(HdRenderParam* renderParam) override;

    void UpdateMaterialResources(HdAuroraMaterial* pMaterial, HdSceneDelegate* pDelegate);

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;

private:
    void UpdateAuroraMaterialPath();

    bool readSTs(VtValue* stOut, HdSceneDelegate* delegate, HdMeshUtil& meshUtil);

    void RebuildAuroraInstances(HdSceneDelegate* delegate);

    bool validateIndices(const VtVec3iArray& indices, int maxAttrCount);

    void ClearAuroraInstances();
    void UpdateAuroraSceneBounds(const VtVec3fArray& points);

    unique_ptr<HdAuroraMeshVertexData> _pVertexData;
    vector<VtVec2fArray> _layerUVData;

    HdAuroraRenderDelegate* _owner;
    Aurora::IMaterialPtr _pAuroraMaterial;

    Aurora::Path _auroraMaterialPath;
    Aurora::Path _auroraDefaultMaterialPath;
    std::vector<Aurora::Path> _auroraMaterialLayerPaths;
    Aurora::Paths _auroraInstances;
    Aurora::InstanceDefinitions _instanceData;
    glm::vec3 _displayColor = { 1.f, 1.f, 1.f };
};
