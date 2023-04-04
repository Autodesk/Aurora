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

#include <MaterialXCore/Document.h>
#include <string>

class HdAuroraRenderDelegate;
struct ProcessedMaterialNetwork;

// Macros to convert between USD GF vector/matrix types and GLM.
#define GfVec3ToGLM(_gfVecPtr) (glm::make_vec3(reinterpret_cast<const float*>(_gfVecPtr)))
#define GfVec4ToGLM(_gfVecPtr) (glm::make_vec4(reinterpret_cast<const float*>(_gfVecPtr)))
#define GfVec4ToGLMVec3(_gfVecPtr) (glm::make_vec3(reinterpret_cast<const float*>(_gfVecPtr)))
#define GfMatrix4fToGLM(_gfMtxPtr) (glm::make_mat4(reinterpret_cast<const float*>(_gfMtxPtr)))
#define GLMMat4ToGF(_glmMtxPtr) (*reinterpret_cast<GfMatrix4f*>(_glmMtxPtr))

class HdAuroraMaterial : public HdMaterial
{
public:
    HdAuroraMaterial(SdfPath const& sprimId, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraMaterial() override;
    void initialize(HdSceneDelegate* delegate);

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(
        HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

    // Comvert the provide Hydra Material ID to Aurora Path.
    static const Aurora::Path GetAuroraMaterialPath(const pxr::SdfPath& mtlId)
    {
        return "HdAuroraMaterial/" + mtlId.GetAsString();
    }

private:
    friend class HdAuroraMesh;

    // Process the Hydra material for this material object
    void ProcessHDMaterial(HdSceneDelegate* delegate);
    // Process the Hydra material network for this material object
    bool ProcessHDMaterialNetwork(VtValue& materialValue);
    // Build a MaterialX document string from a Hydra material network.
    bool BuildMaterialXDocumentFromHDNetwork(
        const ProcessedMaterialNetwork& network, string& materialXDocumentOut);
    // Apply the material from Hydra material network to current Aurora material.
    bool ApplyHDNetwork(const ProcessedMaterialNetwork& network);
    // Get the MaterialX document or path associated with the material in Hydra.
    bool GetHDMaterialXDocument(
        HdSceneDelegate* pDelegate, string& materialTypeOut, string& documentOut);
    // Replace diverse named MaterialX to unified target name
    // eg:<standard_surface name="TestThread"> to <standard_surface name="MaterialX">
    void ReplaceMaterialName(string& materialDocument, const string& targetName);
    // Set all the input value of node to zero
    void InitToDefaultValue(
        MaterialX::NodePtr& pNode, Aurora::Properties& materialProperties, bool& isNodegraph);
    // Seperate the MaterialX document to generate materialX doc with default 0 value
    // and related properties value pair
    string SeparateHDMaterialXDocument(
        string& materialDocument, Aurora::Properties& materialProperties);
    // Create a new Aurora material with this material type and document, if required.
    // Does nothing if current material type and document match the ones provided.
    // Create a new aurora material with this material type and document, if required.
    // Does nothing if current material type and document match the ones provided.
    bool SetupAuroraMaterial(const string& materialType, const string& materialDocument);

    Aurora::Path _auroraMaterialPath;
    HdAuroraRenderDelegate* _owner;

    // Current Aurora material type.
    std::string _auroraMaterialType = "";
    // Hash current Aurora material document (store only hash to avoid keeping multiple copies of
    // large documents).
    size_t _auroraMaterialDocumentHash = 0;

    const set<string> _supportedimages = {
        "base_color_image",
        "opacity_image",
        "specular_roughness_image",
        "normal_image",
    };
};
