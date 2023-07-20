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

#include "HdAuroraMaterial.h"

#include <Aurora/Foundation/Utilities.h>

#include <MaterialXFormat/Util.h>
#include <MaterialXFormat/XmlIo.h>

#include "HdAuroraImageCache.h"
#include "HdAuroraMesh.h"
#include "HdAuroraRenderDelegate.h"
#include "HdAuroraTokens.h"

#include <functional>
#include <list>
#include <regex>

#pragma warning(disable : 4506) // inline function warning (from USD but appears in this file)

// Developer flag to disable separating the MaterialX document into a MaterialX document with
// default values and a separate set of name-value pairs. Should always be 1 in production code.
#define HD_AURORA_SEPARATE_MTLX_DOC 1

// Computes the perceived luminance of a color.
static float luminance(const pxr::GfVec3f& value)
{
    static constexpr pxr::GfVec3f kLuminanceFactors = pxr::GfVec3f(0.2125f, 0.7154f, 0.0721f);

    return pxr::GfDot(value, kLuminanceFactors);
}

// Processed Hydra material network.
// TODO: These should be cached to avoid recalculating each time.
struct ProcessedMaterialNetwork
{
    // Map of material nodes by name.
    std::map<std::string, HdMaterialNode*> nodeMap;
    // Name of surface node within material.
    std::string surfaceName;
    // Surface node.
    HdMaterialNode* surfaceNode;
    // Map of network relationships by output node name.
    std::map<std::string, HdMaterialRelationship*> relationships;
};

// Properties require a value to be added to a Uniform block.
static const string paramBindingTemplate = R"(
      <input name="%s" type="%s" value="%s" />)";

static const string nodeGraphBindingTemplate = R"(
      <input name="%s" type="%s" nodegraph="NG%s" output="out1" />)";

static const string nodeGraphImageTemplate = R"(
  <nodegraph name="NG%s">
    <image name="%s" type="%s">
      <parameter name="file" type="filename" value="" />
      <parameter name="uaddressmode" type="string" value="periodic" />
      <parameter name="vaddressmode" type="string" value="periodic" />
    </image>
    <output name="out1" type="%s" nodename="%s" />
  </nodegraph>)";

static const string nodeGraphNormalMapTemplate = R"(
  <nodegraph name="NG%s">
    <image name="%s_image" type="vector3">
      <parameter name="file" type="filename" value="" />
      <parameter name="uaddressmode" type="string" value="periodic" />
      <parameter name="vaddressmode" type="string" value="periodic" />
    </image>
    <normalmap name="%s" type="vector3">
      <input name="in" type="vector3" nodename="%s_image" />
    </normalmap>
    <output name="out1" type="vector3" nodename="%s" />
  </nodegraph>)";

static const string materialDefinitionTemplate = R"(
<?xml version = "1.0" ?>
<materialx version = "1.38"> %s
  <standard_surface name="SS_Material" type="surfaceshader"> %s
  </standard_surface>
  <surfacematerial name="SS_Material_shader" type="material">
    <input name="surfaceshader" type="surfaceshader" nodename="SS_Material" />
  </surfacematerial>
  <look name="default">
    <materialassign name="material_assign_default" material="SS_Material_shader" geom="*" />
  </look>
</materialx>)";

HdAuroraMaterial::HdAuroraMaterial(SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate) :
    HdMaterial(rprimId), _owner(renderDelegate)
{
}

HdAuroraMaterial::~HdAuroraMaterial() {}

HdDirtyBits HdAuroraMaterial::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllSceneDirtyBits;
}
void HdAuroraMaterial::InitToDefaultValue(
    MaterialX::NodePtr& pNode, Aurora::Properties& materialProperties, bool& isNodegraph)
{
    if (!pNode || !pNode->getInputCount())
    {
        return;
    }
    std::pair<std::string, std::string> inputValue;
    for (MaterialX::InputPtr input : pNode->getInputs())
    {
        std::string namePath;

        // Input is part of standard_surface, directly store its name
        // Otherwise store the namepath to know the hierarchical nodegraph name
        if (isNodegraph)
        {
            string ngNamepath            = input->getNamePath();
            const size_t first_slash_idx = ngNamepath.find('/');
            if (0 != first_slash_idx)
            {
                // Get rid of the first path of namepath, namely the name of nodegraph
                namePath = ngNamepath.substr(first_slash_idx + 1, ngNamepath.length());
            }
        }
        else
        {
            namePath = input->getName();
        }
        const std::string type          = input->getType();
        const MaterialX::ValuePtr value = input->getValue();

        // Skip invalid input type and value
        if (type.empty() || !value)
            continue;

        // Reset all non-zero value to zero according to their types
        if (type == MaterialX::getTypeString<float>())
        {
            if (value->asA<float>() != 0.0f)
            {
                materialProperties[namePath] = value->asA<float>();
                input->setValue(0.0f);
            }
        }
        else if (type == MaterialX::getTypeString<int>())
        {
            if (value->asA<int>() != 0)
            {
                // TODO: The "which" value of "switch" node is kept for now, as it is not currently
                // supported by the data-driven mechanism.
                auto pParent = input->getParent();
                if (pParent && pParent->getCategory() == "switch")
                {
                    const size_t last_slash_idx = namePath.find_last_of('/');
                    if (string::npos != last_slash_idx)
                    {
                        auto parameterName = namePath.substr(last_slash_idx + 1, namePath.length());
                        if ("which" == parameterName)
                            continue;
                    }
                }

                materialProperties[namePath] = value->asA<int>();
                input->setValue(0);
            }
        }
        else if (type == MaterialX::getTypeString<MaterialX::Vector2>())
        {
            if (value->asA<MaterialX::Vector2>() != MaterialX::Vector2(0.0f, 0.0f))
            {
                glm::vec2 val;
                val[0]                       = value->asA<MaterialX::Vector2>()[0];
                val[1]                       = value->asA<MaterialX::Vector2>()[1];
                materialProperties[namePath] = val;
                input->setValue(MaterialX::Vector2(0.0f, 0.0f));
            }
        }
        else if (type == MaterialX::getTypeString<MaterialX::Vector3>())
        {
            if (value->asA<MaterialX::Vector3>() != MaterialX::Vector3(0.0f, 0.0f, 0.0f))
            {
                glm::vec3 val;
                val[0]                       = value->asA<MaterialX::Vector3>()[0];
                val[1]                       = value->asA<MaterialX::Vector3>()[1];
                val[2]                       = value->asA<MaterialX::Vector3>()[2];
                materialProperties[namePath] = val;
                input->setValue(MaterialX::Vector3(0.0f, 0.0f, 0.0f));
            }
        }
        else if (type == MaterialX::getTypeString<MaterialX::Vector4>())
        {
            if (value->asA<MaterialX::Vector4>() != MaterialX::Vector4(0.0f, 0.0f, 0.0f, 0.0f))
            {
                glm::vec4 val;
                val[0]                       = value->asA<MaterialX::Vector4>()[0];
                val[1]                       = value->asA<MaterialX::Vector4>()[1];
                val[2]                       = value->asA<MaterialX::Vector4>()[2];
                val[3]                       = value->asA<MaterialX::Vector4>()[3];
                materialProperties[namePath] = val;
                input->setValue(MaterialX::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
            }
        }
        else if (type == MaterialX::getTypeString<MaterialX::Color3>())
        {
            if (value->asA<MaterialX::Color3>() != MaterialX::Color3(0.0f, 0.0f, 0.0f))
            {
                glm::vec3 val;
                val[0]                       = value->asA<MaterialX::Color3>()[0];
                val[1]                       = value->asA<MaterialX::Color3>()[1];
                val[2]                       = value->asA<MaterialX::Color3>()[2];
                materialProperties[namePath] = val;
                input->setValue(MaterialX::Color3(0.0f, 0.0f, 0.0f));
            }
        }
        else if (type == MaterialX::getTypeString<bool>())
        {
            if (value->asA<bool>() != false)
            {
                materialProperties[namePath] = value->asA<bool>();
                input->setValue(false);
            }
        }
    }
}
void HdAuroraMaterial::ReplaceMaterialName(string& materialDocument, const string& targetName)
{
    std::string originalMaterialName;
    MaterialX::DocumentPtr originalDoc = MaterialX::createDocument();
    MaterialX::readFromXmlString(originalDoc, materialDocument);
    for (MaterialX::ElementPtr elem : originalDoc->traverseTree())
    {
        if (elem->isA<MaterialX::Node>())
        {
            MaterialX::NodePtr pNode = elem->asA<MaterialX::Node>();
            if (pNode->getType() == MaterialX::SURFACE_SHADER_TYPE_STRING)
            {
                originalMaterialName = pNode->getName();
                break;
            }
        }
    }
    if (!originalMaterialName.empty())
    {
        std::regex srcNodegraphName("nodegraph=\"" + originalMaterialName);
        std::regex srcNodeName("name=\"" + originalMaterialName);
        string processedMtlXString =
            regex_replace(materialDocument, srcNodegraphName, "nodegraph=\"" + targetName);
        materialDocument = regex_replace(processedMtlXString, srcNodeName, "name=\"" + targetName);
    }
}
string HdAuroraMaterial::SeparateHDMaterialXDocument(
    string& materialDocument, Aurora::Properties& materialProperties)
{
    // Step1: Unified naming
    std::string targetName = "HdAuroraMaterialX";
    ReplaceMaterialName(materialDocument, targetName);

    // Step2: Collect name-value pair
    MaterialX::DocumentPtr hdMaterialXDoc = MaterialX::createDocument();
    MaterialX::readFromXmlString(hdMaterialXDoc, materialDocument);
    bool isNodeGraph = false;
    for (MaterialX::ElementPtr elem : hdMaterialXDoc->traverseTree())
    {
        if (elem->isA<MaterialX::Node>())
        {
            MaterialX::NodePtr pNode = elem->asA<MaterialX::Node>();
            InitToDefaultValue(pNode, materialProperties, isNodeGraph);
        }
        else if (elem->isA<MaterialX::NodeGraph>())
        {
            MaterialX::NodeGraphPtr pNodeGraph = elem->asA<MaterialX::NodeGraph>();
            isNodeGraph                        = true;
            for (MaterialX::ElementPtr subElem : pNodeGraph->traverseTree())
            {
                MaterialX::NodePtr pNode = subElem->asA<MaterialX::Node>();
                InitToDefaultValue(pNode, materialProperties, isNodeGraph);
            }
        }
    }
    return MaterialX::writeToXmlString(hdMaterialXDoc);
}
bool HdAuroraMaterial::SetupAuroraMaterial(
    const string& materialType, const string& materialDocument)
{
    // Calculate hash of document.
    std::size_t docHash = std::hash<std::string> {}(materialDocument);
    auto pRenderer      = _owner->GetRenderer();
    auto pScene         = _owner->GetScene();

    // If we already have this material type and document just return without doing anything.
    if (_auroraMaterialType.compare(materialType) == 0 && _auroraMaterialDocumentHash == docHash)
    {
        // This comparison should only be true if we already have a material.
        AU_ASSERT(!_auroraMaterialPath.empty(), "Failed to create Aurora material");

        return false;
    }

    // Calculate Aurora path from Hydra ID.
    _auroraMaterialPath = GetAuroraMaterialPath(GetId());

    // Set the material type for material (this will create it if needed.)
    pScene->setMaterialType(_auroraMaterialPath, materialType, materialDocument);

    // Set the material type and document hash.
    _auroraMaterialDocumentHash = docHash;
    _auroraMaterialType         = materialType;

    return true;
}

void HdAuroraMaterial::ProcessHDMaterial(HdSceneDelegate* delegate)
{
    const auto& id   = GetId();
    VtValue hdMatVal = delegate->GetMaterialResource(id);
    auto pRenderer   = _owner->GetRenderer();

    string hdMaterialXType;
    string hdMaterialXDocument;
    if (GetHDMaterialXDocument(delegate, hdMaterialXType, hdMaterialXDocument))
    {
        Aurora::Properties materialProperties;

#if HD_AURORA_SEPARATE_MTLX_DOC
        if (!hdMaterialXDocument.empty())
        {
            // Separate hdMaterialXDocument into default doc and name-value pair
            hdMaterialXDocument =
                SeparateHDMaterialXDocument(hdMaterialXDocument, materialProperties);
        }
#endif

        // Send the document to Aurora (with the parameters reset.)
        SetupAuroraMaterial(hdMaterialXType, hdMaterialXDocument);

        // Send the name-value pair by IScene::setMaterialProperties
        _owner->GetScene()->setMaterialProperties(_auroraMaterialPath, materialProperties);
    }
    else if (!ProcessHDMaterialNetwork(hdMatVal))
    {
        Aurora::Properties materialProperties = { { "base_color_image", nullptr },
            { "base_color", nullptr } };

        // If we don't have a material network, create preview surface using default built-in
        // shader.
        SetupAuroraMaterial(Aurora::Names::MaterialTypes::kBuiltIn, "Default");

        // If no material network read from the Hydra material parameters.
        // These parameters  are set as overrides over what is specified in the
        // MaterialX document.
        std::vector<std::pair<std::string, std::string>> knownMapParameters = {
            { "base_color_map", "base_color_image" },
            { "roughness_map", "specular_roughness_image" }
            //,{ "coat_map", "coat_color_image" }
            //,{ "coat_rough_map", "coat_roughness_image" }
            ,
            { "bump_map", "normal_image" }
            //,{ "displacement_map", "displacement_image" }
        };
        for (auto inputName : knownMapParameters)
        {
            VtValue texFilenameVal = delegate->Get(id, pxr::TfToken(inputName.first));
            auto texFilenamePath   = texFilenameVal.Get<pxr::SdfAssetPath>();
            string texFilename     = texFilenamePath.GetAssetPath();
            bool forceLinear       = inputName.second.compare("normal_image") == 0;

            if (texFilename.size() > 0)
            {
                Aurora::Path auroraImagePath =
                    _owner->imageCache().acquireImage(texFilename, false, forceLinear);

                materialProperties[inputName.second] = auroraImagePath;
            }
        }

        std::vector<std::pair<std::string, std::string>> knownFloatParameters = {
            { "roughness", "specular_roughness" }, { "base_weight", "base" },
            { "reflectivity", "specular" }, { "metalness", "metalness" },
            { "anisotrophy", "specular_anisotropy" }, { "anisoangle", "specular_rotation" },
            { "transparency", "transmission" }, { "trans_depth", "transmission_depth" },
            { "emission", "emission" }, { "diff_roughness", "diffuse_roughness" },
            { "coating", "coat" }, { "coat_roughness", "coat_roughness" },
            { "coat_ior", "coat_IOR" }
        };
        for (auto inputName : knownFloatParameters)
        {
            pxr::VtValue val = delegate->Get(id, pxr::TfToken(inputName.first));
            if (val.IsHolding<float>())
            {
                float fVal                           = val.Get<float>();
                materialProperties[inputName.second] = fVal;
            }
        }

        std::vector<std::pair<std::string, std::string>> knownFloat4Parameters = {
            { "base_color", "base_color" }, { "refl_color", "specular_color" },
            { "trans_color", "transmission_color" }, { "emit_color", "emission_color" },
            { "coat_color", "coat_color" }
        };
        for (auto inputName : knownFloat4Parameters)
        {
            pxr::VtValue val = delegate->Get(id, pxr::TfToken(inputName.first));
            if (val.IsHolding<GfVec4f>())
            {
                GfVec4f color                        = val.Get<GfVec4f>();
                materialProperties[inputName.second] = GfVec4ToGLMVec3(&color);
            }
        }

        _owner->GetScene()->setMaterialProperties(_auroraMaterialPath, materialProperties);
    }
}

bool HdAuroraMaterial::GetHDMaterialXDocument(
    HdSceneDelegate* pDelegate, string& materialTypeOut, string& documentOut)
{
    SdfPath id = GetId();

    // Return value indicating if materialX is used to create the material.
    // This will be set to true if a materialX document is loaded from file or from string.
    bool isMaterialX = false;

    // Default Aurora material type is default built-in material.
    std::string materialType     = Aurora::Names::MaterialTypes::kBuiltIn;
    std::string materialDocument = "Default";

    VtValue materialXFilePath = pDelegate->Get(id, HdAuroraTokens::kMaterialXFilePath);

    // First we look to see if there is a document by file name.
    if (!materialXFilePath.IsEmpty())
    {
        // Set Aurora material type and document for materialX path.
        materialDocument = materialXFilePath.Get<pxr::SdfAssetPath>().GetAssetPath();
        materialType     = Aurora::Names::MaterialTypes::kMaterialXPath;

        isMaterialX = true;
    }
    else
    {
        // If we don't find a filename property we look for a document string property.
        VtValue materialXDocument = pDelegate->Get(id, HdAuroraTokens::kMaterialXDocument);
        if (!materialXDocument.IsEmpty() && materialXDocument.IsHolding<string>())
        {
            // Set Aurora material type and document for materialX path.
            materialDocument = materialXDocument.UncheckedGet<string>();
            materialType     = Aurora::Names::MaterialTypes::kMaterialX;
            isMaterialX      = true;
        }
    }

    materialTypeOut = materialType;
    documentOut     = materialDocument;

    return isMaterialX;
}

void HdAuroraMaterial::Sync(
    HdSceneDelegate* delegate, HdRenderParam* /* renderParam */, HdDirtyBits* dirtyBits)
{

    if (_auroraMaterialPath.empty() || (*dirtyBits & HdMaterial::DirtyResource) ||
        (*dirtyBits & HdMaterial::DirtyParams))
    {
        // Note: We reprocess the network even for parameter changes.
        // A new Aurora Material is only generated if the hash of the material changes, otherwise
        // only values are updated. In the future this could be more optimal by caching the network
        // for less processing when parameters change only.
        ProcessHDMaterial(delegate);

        _owner->SetSampleRestartNeeded(true);
    }

    *dirtyBits &= ~HdMaterial::AllDirty;
}

void HdAuroraMaterial::initialize(HdSceneDelegate* delegate)
{
    if (_auroraMaterialPath.empty())
    {
        ProcessHDMaterial(delegate);
    }
}

bool HdAuroraMaterial::BuildMaterialXDocumentFromHDNetwork(
    const ProcessedMaterialNetwork& network, string& materialXDocumentOut)
{
    // MaterialX node graph XML string.
    string nodeGraphsStr;
    // MaterialX parameter binding XML string.
    string parameterBindingStr;

    // Node graph index.
    int ngIndex = 1;

    // Float3 setter function to convert Hydra GfVec3f values to Aurora values
    auto setF3Value = [](string& parameterBindingStr, const VtValue& vtVal,
                          const std::string& auroraName) {
        if (vtVal.IsHolding<GfVec3f>())
        {
            // Add the input binding for input to the dynamically built materialX document.
            // Note: Values are applied separately and not built into the document, so value is all
            // zeros here.
            parameterBindingStr += Aurora::Foundation::sFormat(
                paramBindingTemplate, auroraName.c_str(), "color3", "0.0,0.0,0.0");
        }
    };

    // Float setter function to convert Hydra float values to Aurora values
    static auto setF1Value = [](string& parameterBindingStr, const VtValue& vtVal,
                                 const std::string& auroraName) {
        if (vtVal.IsHolding<float>())
        {
            // Add the input binding for input to the dynamically built materialX document.
            // Note: Values are applied separately and not built into the document, so value is zero
            // here.
            parameterBindingStr += Aurora::Foundation::sFormat(
                paramBindingTemplate, auroraName.c_str(), "float", "0.0");
        }
    };

    // Float setter function to convert Hydra float values to Aurora values
    static auto setOpacityValue = [](string& parameterBindingStr, const VtValue& vtVal,
                                      const std::string& auroraName) {
        if (vtVal.IsHolding<float>())
        {
            // Add the input binding for input to the dynamically built materialX document.
            // Note: Values are applied separately and not built into the document, so value is zero
            // here.
            parameterBindingStr += Aurora::Foundation::sFormat(
                paramBindingTemplate, auroraName.c_str(), "float", "0.0");
        }
        else if (vtVal.IsHolding<GfVec3f>())
        {
            // The opacity value can also be passed as a GfVec3f, which must be converted to float.
            // all zeros here.
            parameterBindingStr += Aurora::Foundation::sFormat(
                paramBindingTemplate, auroraName.c_str(), "float", "0.0");
        }
    };

    // Parameter information struct used by translation callback.
    struct ParameterInfo
    {
        TfToken token;
        std::string auroraName;
        std::function<void(std::string&, const VtValue&, const std::string&)> converter;
    };

    // A generic lambda function to translate properties from hydra to Aurora
    // token is the name of the Hydra value to get
    // auroraName is the name of the Aurora parameter to set
    // setValueFn is the function that converts the type specific VtValue
    // to an appropriate Aurora function call.
    auto translate = [&network, &nodeGraphsStr, &parameterBindingStr, &ngIndex, this](
                         const ParameterInfo& parameterInfo) {
        if (network.surfaceNode->parameters.find(parameterInfo.token) !=
            network.surfaceNode->parameters.end())
        {
            // Set HdAurora's material property using the property converter
            VtValue& colorVal = network.surfaceNode->parameters[parameterInfo.token];
            if (parameterInfo.converter)
                parameterInfo.converter(parameterBindingStr, colorVal, parameterInfo.auroraName);
        }
        else
        {
            static const TfToken fileTok("file");

            // If no parameter look for a output connected to the input for this node.
            std::string paramOutputName =
                network.surfaceName + ":" + parameterInfo.token.GetString();
            auto relIter = network.relationships.find(paramOutputName);
            if (relIter != network.relationships.end())
            {
                const HdMaterialRelationship* paramRelationship = relIter->second;
                // Get the file node connected to parameter being translated/
                auto nodeIter = network.nodeMap.find(paramRelationship->inputId.GetString());
                if (nodeIter != network.nodeMap.end())
                {
                    auto fileNode = nodeIter->second;
                    // Try and get the file parameter
                    auto paramIter = fileNode->parameters.find(fileTok);
                    if (paramIter != fileNode->parameters.end() &&
                        paramIter->second.IsHolding<SdfAssetPath>())
                    {
                        // Get the texture path from the node.
                        SdfAssetPath texturePath = paramIter->second.Get<SdfAssetPath>();
                        string filename          = texturePath.GetResolvedPath();

                        // Keep track of unique materials and node graphs.
                        string indexStr = std::to_string(ngIndex++);

                        // Set the Aurora image property
                        string paramName = parameterInfo.auroraName + "_image";
                        if (_supportedimages.find(paramName) != _supportedimages.end())
                        {

                            // Choose type based on outputName
                            string txtType  = "color3";
                            auto outputName = paramRelationship->outputName.GetString();

                            if (outputName.compare("normal") == 0)
                            {
                                txtType = "vector3";
                                // Add the texture nodegraph and the input binding for each textured
                                // input to the dynamically built materialX document.
                                nodeGraphsStr += Aurora::Foundation::sFormat(
                                    nodeGraphNormalMapTemplate, indexStr.c_str(), paramName.c_str(),
                                    paramName.c_str(), paramName.c_str(), paramName.c_str());
                            }
                            else
                            {
                                if (outputName.compare("diffuseColor") == 0)
                                    txtType = "color3";
                                else
                                    txtType = "float";

                                // Add the texture nodegraph and the input binding for each textured
                                // input to the dynamically built materialX document.
                                nodeGraphsStr += Aurora::Foundation::sFormat(nodeGraphImageTemplate,
                                    indexStr.c_str(), paramName.c_str(), txtType.c_str(),
                                    txtType.c_str(), paramName.c_str());
                            }
                            parameterBindingStr += Aurora::Foundation::sFormat(
                                nodeGraphBindingTemplate, parameterInfo.auroraName.c_str(),
                                txtType.c_str(), indexStr.c_str());
                        }
                    }
                }
            }
        }
    };

    // Get setting for mapping opacity to transmission (default to true if not set.)
    VtValue mapOpacityToTransmissionVal =
        _owner->GetRenderSetting(HdAuroraTokens::kMapMaterialOpacityToTransmission);
    bool mapOpacityToTransmission = mapOpacityToTransmissionVal.IsHolding<bool>()
        ? mapOpacityToTransmissionVal.Get<bool>()
        : true;

    // Setup the opacity parameter to map UsdPreviewSurface opacity to Standard Surface
    // transmission, as required by kMapMaterialOpacityToTransmission setting.
    ParameterInfo opacityParameterInfo = { TfToken("opacity"), "transmission", setOpacityValue };
    if (!mapOpacityToTransmission)
        opacityParameterInfo = { TfToken("opacity"), "opacity", setF3Value };

    // Specify the named pair of parameters to translate from Hydra to Aurora
    // and the translation function to use for the parameter.
    // { Hydra token, Aurora name (standard_surface), translation function }
    // clang-format off
    static const std::vector<ParameterInfo> parameterMap =
    {
        { TfToken("diffuseColor"),          "base_color",         setF3Value },
        { TfToken("specularColor"),         "specular_color",     setF3Value },
        { TfToken("roughness"),             "specular_roughness", setF1Value },
        { TfToken("ior"),                   "specular_IOR",       setF1Value },
        { TfToken("emissiveColor"),         "emission_color",     setF3Value },
        opacityParameterInfo,
        { TfToken("clearcoat"),             "coat",               setF1Value },
        { TfToken("clearcoatColor"),        "coat_color",         setF3Value },
        { TfToken("clearcoatRoughness"),    "coat_roughness",     setF1Value },
        { TfToken("metallic"),              "metalness",          setF1Value },
        { TfToken("normal"),                "normal",             nullptr } // No converter function, can only be a texture.
    };
    // clang-format on

    // translate all the supported parameters in the map declared above
    for (auto paramInfo : parameterMap)
        translate(paramInfo);

    // Build the MaterialX document by string.
    materialXDocumentOut = Aurora::Foundation::sFormat(
        materialDefinitionTemplate, nodeGraphsStr.c_str(), parameterBindingStr.c_str());

    return true;
}

bool HdAuroraMaterial::ApplyHDNetwork(const ProcessedMaterialNetwork& network)
{
    // Float3 setter function to convert Hydra GfVec3f values to Aurora values
    auto setF3Value = [](Aurora::Properties& materialProperties, const VtValue& vtVal,
                          const std::string& auroraName) {
        if (vtVal.IsHolding<GfVec3f>())
        {
            GfVec3f val                    = vtVal.UncheckedGet<GfVec3f>();
            materialProperties[auroraName] = GfVec3ToGLM(&val);
        }
    };

    // Setter to apply float1 to float3 Aurora value.
    [[maybe_unused]] auto setF1ToF3Value = [](Aurora::Properties& materialProperties,
                                               const VtValue& vtVal,
                                               const std::string& auroraName) {
        if (vtVal.IsHolding<float>())
        {
            float val                      = vtVal.UncheckedGet<float>();
            materialProperties[auroraName] = glm::vec3(val, val, val);
        }
    };

    // Setter to apply UsdPreviewSurface opacity to standard surface transmission.
    auto setTransmissionFromOpacity = [](Aurora::Properties& materialProperties,
                                          const VtValue& vtVal, const std::string& auroraName) {
        if (vtVal.IsHolding<float>())
        {
            float val                      = vtVal.UncheckedGet<float>();
            materialProperties[auroraName] = 1.0f - val; // Transmission is one minus opacity.
        }
        else if (vtVal.IsHolding<GfVec3f>())
        {
            // Opacity can be passed as GfVec3f, in which case it should be converted to float.
            GfVec3f val                    = vtVal.UncheckedGet<GfVec3f>();
            float floatVal                 = luminance(val);
            materialProperties[auroraName] = 1.0f - floatVal; // Transmission is one minus opacity.
        }
    };

    // Custom setter function to convert Hydra Preview Surface specular color to SS.
    static auto setSpecularColor = [](Aurora::Properties& materialProperties, const VtValue& vtVal,
                                       const std::string& auroraName) {
        if (vtVal.IsHolding<GfVec3f>())
        {
            GfVec3f val                    = vtVal.UncheckedGet<GfVec3f>();
            val[0]                         = min(std::sqrt(std::sqrt(val[0])) * 2.0f, 1.0f);
            val[1]                         = min(std::sqrt(std::sqrt(val[1])) * 2.0f, 1.0f);
            val[2]                         = min(std::sqrt(std::sqrt(val[2])) * 2.0f, 1.0f);
            materialProperties[auroraName] = GfVec3ToGLM(&val);
        }
    };

    // Float setter function to convert Hydra float values to Aurora values
    static auto setF1Value = [](Aurora::Properties& materialProperties, const VtValue& vtVal,
                                 const std::string& auroraName) {
        if (vtVal.IsHolding<float>())
        {
            float val                      = vtVal.UncheckedGet<float>();
            materialProperties[auroraName] = val;
        }
    };

    // Parameter information struct used by apply callback.
    struct ParameterInfo
    {
        TfToken token;
        std::string auroraName;
        std::function<void(Aurora::Properties&, const VtValue&, const std::string&)> converter;
    };

    Aurora::Properties materialProperties;

    // A generic lambda function to translate properties from Hydra to Aurora
    // token is the name of the Hydra value to get
    // auroraName is the name of the Aurora parameter to set
    // setValueFn is the function that converts the type specific VtValue
    // to an appropriate Aurora function call.
    auto apply = [&network, &materialProperties, this](const ParameterInfo& parameterInfo) {
        if (network.surfaceNode->parameters.find(parameterInfo.token) !=
            network.surfaceNode->parameters.end())
        {
            // Set HdAurora's material property using the property converter
            VtValue& colorVal = network.surfaceNode->parameters[parameterInfo.token];
            if (parameterInfo.converter)
                parameterInfo.converter(materialProperties, colorVal, parameterInfo.auroraName);
        }
        else
        {
            static const TfToken fileTok("file");

            // If no parameter look for a output connected to the input for this node.
            std::string paramOutputName =
                network.surfaceName + ":" + parameterInfo.token.GetString();
            auto relIter = network.relationships.find(paramOutputName);
            if (relIter != network.relationships.end())
            {
                const HdMaterialRelationship* paramRelationship = relIter->second;
                // Get the file node connected to parameter being translated/
                auto nodeIter = network.nodeMap.find(paramRelationship->inputId.GetString());
                if (nodeIter != network.nodeMap.end())
                {
                    auto fileNode = nodeIter->second;
                    // Try and get the file parameter
                    auto paramIter = fileNode->parameters.find(fileTok);
                    if (paramIter != fileNode->parameters.end() &&
                        paramIter->second.IsHolding<SdfAssetPath>())
                    {
                        // Get the texture path from the node.
                        SdfAssetPath texturePath = paramIter->second.Get<SdfAssetPath>();
                        string filename          = texturePath.GetResolvedPath();

                        // Set the Aurora image property
                        string paramName = parameterInfo.auroraName + "_image";
                        if (_supportedimages.find(paramName) != _supportedimages.end())
                        {
                            bool forceLinear = parameterInfo.auroraName.compare("normal") == 0;
                            materialProperties[paramName] =
                                _owner->imageCache().acquireImage(filename, false, forceLinear);
                        }
                    }
                }
            }
        }
    };

    // Get setting for mapping opacity to transmission (default to true if not set.)
    VtValue mapOpacityToTransmissionVal =
        _owner->GetRenderSetting(HdAuroraTokens::kMapMaterialOpacityToTransmission);
    bool mapOpacityToTransmission = mapOpacityToTransmissionVal.IsHolding<bool>()
        ? mapOpacityToTransmissionVal.Get<bool>()
        : true;

    // Setup the opacity parameter to map UsdPreviewSurface opacity to Standard Surface
    // transmission, as required by kMapMaterialOpacityToTransmission setting.
    ParameterInfo opacityParameterInfo = { TfToken("opacity"), "transmission",
        setTransmissionFromOpacity };
    if (!mapOpacityToTransmission)
        opacityParameterInfo = { TfToken("opacity"), "opacity", setF3Value };

    // Specify the named pair of parameters to translate from Hydra to Aurora
    // and the translation function to use for the parameter.
    // { Hydra token, Aurora name (standard_surface), translation function }
    // clang-format off
    static const std::vector<ParameterInfo> parameterMap =
    {
        { TfToken("diffuseColor"),          "base_color",         setF3Value },
        { TfToken("specularColor"),         "specular_color",     setSpecularColor },
        { TfToken("roughness"),             "specular_roughness", setF1Value },
        { TfToken("ior"),                   "specular_IOR",       setF1Value },
        { TfToken("emissiveColor"),         "emission_color",     setF3Value },
        opacityParameterInfo,
        { TfToken("clearcoat"),             "coat",               setF1Value },
        { TfToken("clearcoatColor"),        "coat_color",         setF3Value },
        { TfToken("clearcoatRoughness"),    "coat_roughness",     setF1Value },
        { TfToken("metallic"),              "metalness",          setF1Value },
        { TfToken("normal"),                "normal",             nullptr }
    };
    // clang-format on

    // translate all the supported parameters in the map declared above
    for (auto paramInfo : parameterMap)
        apply(paramInfo);

    _owner->GetScene()->setMaterialProperties(_auroraMaterialPath, materialProperties);

    return true;
}

bool HdAuroraMaterial::ProcessHDMaterialNetwork(VtValue& materialValue)
{
    // Get the network map associated with this Hydra material.
    if (!materialValue.IsHolding<HdMaterialNetworkMap>())
        return false;
    auto networkMap = materialValue.UncheckedGet<HdMaterialNetworkMap>();

    // Process the Hydra material network.
    ProcessedMaterialNetwork processedNetwork;

    // Get terminals for the network.
    if (networkMap.terminals.size() == 0)
    {
        TF_WARN("No terminals in material network map for material " + GetId().GetString());
        return false;
    }

    // The first terminal is assumed to be the name of the surface node in the network
    auto outputTerminal          = networkMap.terminals[0];
    processedNetwork.surfaceName = outputTerminal.GetString();

    // Get the network for surface (not to be confused with the surface node)
    auto networkIter = networkMap.map.find(pxr::HdMaterialTerminalTokens->surface);
    if (networkIter == networkMap.map.end())
    {
        TF_WARN("No surface network in material network map for material " + GetId().GetString());
        return false;
    }
    auto network = networkIter->second;

    // GAM TODO: We should be able to cache a lot of this stuff so we aren't rebuilding all these
    // structures every time you set a color Build lookup table of nodes in network
    for (size_t n = 0; n < network.nodes.size(); n++)
    {
        HdMaterialNode& node                            = network.nodes[n];
        processedNetwork.nodeMap[node.path.GetString()] = &network.nodes[n];
    }
    // Build lookup table of relationships in network
    for (size_t i = 0; i < network.relationships.size(); i++)
    {
        auto rel = network.relationships[i];
        // Combine output ID and name to create key
        processedNetwork
            .relationships[rel.outputId.GetString() + ":" + rel.outputName.GetString()] =
            &network.relationships[i];
    }

    // Get the node for surface itself
    processedNetwork.surfaceNode = processedNetwork.nodeMap[outputTerminal.GetString()];
    if (!processedNetwork.surfaceNode)
    {
        TF_WARN(
            "No surface node for output terminal in network for material " + GetId().GetString());
        return false;
    }

    // Build a materialX document string from the processed network.
    string mtlXDocument;
    BuildMaterialXDocumentFromHDNetwork(processedNetwork, mtlXDocument);

    // Setup the material from the materialX document. This will recreate the material if there are
    // any changes.
    SetupAuroraMaterial(Aurora::Names::MaterialTypes::kMaterialX, mtlXDocument);

    // Setup the material from the materialX document, setting the parameters on the Aurora
    // material.
    ApplyHDNetwork(processedNetwork);

    return true;
}
