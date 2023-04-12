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

#include "MaterialGenerator.h"

#include <fstream>
#include <iostream>

#include "MaterialBase.h"

namespace Aurora
{
namespace MaterialXCodeGen
{

static string mapMaterialXTextureName(const string& name)
{
    string mappedName = "";

    // Any image input containing string 'diffuse' or 'base' is mapped to the
    // base_color_image texture (e.g. "generic_diffuse_file" or "base_color_image".).
    if (name.find("diffuse") != string::npos || name.find("base") != string::npos)
    {
        mappedName = "base_color_image";
    }
    // Any image input containing string 'roughness' or 'specular' is mapped to the
    // specular_roughness_image texture.
    else if (name.find("roughness") != string::npos || name.find("specular") != string::npos)
    {
        mappedName = "specular_roughness_image";
    }
    // Any image input containing string 'normal' or 'bump' is mapped to the
    // normal_image texture.
    else if (name.find("normal") != string::npos || name.find("bump") != string::npos)
    {
        mappedName = "normal_image";
    }
    // Any image input containing string 'opacity', 'alpha', 'cutout', or 'transmission'
    // is mapped to the opacity_image texture.
    else if (name.find("opacity") != string::npos || name.find("alpha") != string::npos ||
        name.find("cutout") != string::npos || name.find("transmission") != string::npos)
    {
        mappedName = "opacity_image";
    }
    // If nothing else matches, then map any image containing color to base_color_image
    // texture.
    else if (name.find("color") != string::npos)
    {
        mappedName = "base_color_image";
    }

    return mappedName;
}

// Extremely primitive GLSL-to-HLSL conversion function, that handles cases not dealt with in shader
// code prefix.
// TODO: This is a very basic placeholder, we need a better solution for this.
static string GLSLToHLSL(const string& glslStr)
{
    // Replace vectors initialized by scalar 0.0.
    string hlslStr = regex_replace(glslStr, regex("vec4\\(0.0\\)"), "vec4(0.0,0.0,0.0,0.0)");
    hlslStr        = regex_replace(hlslStr, regex("vec3\\(0.0\\)"), "vec3(0.0,0.0,0.0)");
    hlslStr        = regex_replace(hlslStr, regex("vec2\\(0.0\\)"), "vec2(0.0,0.0)");

    // Replace GLSL float array initializers.
    hlslStr = regex_replace(hlslStr, regex("float\\[.*\\]\\((.+)\\);\\n"), "{$1};\n");

    return hlslStr;
}

MaterialGenerator::MaterialGenerator(IRenderer* pRenderer, const string& mtlxFolder) :
    _pRenderer(pRenderer)
{
    // Create code generator.
    _pCodeGenerator = make_unique<MaterialXCodeGen::BSDFCodeGenerator>(mtlxFolder);

    // Map of MaterialX output parameters to Aurora material property.
    // Used by PTRenderer::generateMaterialX.
    // clang-format off
    _bsdfInputParamMapping =
    {
        { "base", "base" },
        { "base_color", "baseColor" },
        { "coat", "coat" },
        { "coat_color", "coatColor" },
        { "coat_roughness", "coatRoughness" },
        { "coat_anisotropy", "coatAnisotropy" },
        { "coat_rotation", "coatRotation" },
        { "coat_IOR", "coatIOR" },
        { "diffuse_roughness", "diffuseRoughness" },
        { "metalness", "metalness" },
        { "specular", "specular" },
        { "specular_color", "specularColor" },
        { "specular_roughness", "specularRoughness" },
        { "specular_anisotropy", "specularAnisotropy" },
        { "specular_IOR", "specularIOR" },
        { "specular_rotation", "specularRotation" },
        { "transmission", "transmission" },
        { "transmission_color", "transmissionColor" },
        { "subsurface", "subsurface" },
        { "subsurfaceColor", "subsurfaceColor" },
        { "subsurface_scale", "subsurfaceScale" },
        { "subsurface_radius", "subsurfaceRadius" },
        { "sheen", "sheen" },
        { "sheen_color", "sheenColor" },
        { "sheen_roughness", "sheenRoughness" },
        { "coat", "coat" },
        { "coat_color", "coatColor" },
        { "coat_roughness", "coatRoughness" },
        { "coat_anisotropy", "coatAnisotropy" },
        { "coat_rotation", "coatRotation" },
        { "coat_IOR", "coatIOR" },
        { "coat_affect_color", "coatAffectColor" },
        { "coat_affect_roughness", "coatAffectRoughness" },
        { "opacity", "opacity" },
        { "base_color_image_scale", "baseColorTexTransform.scale" },
        { "base_color_image_offset", "baseColorTexTransform.offset" },
        { "base_color_image_rotation", "baseColorTexTransform.rotation" },
        { "opacity_image_scale", "opacityTexTransform.scale" },
        { "opacity_image_offset", "opacityTexTransform.offset" },
        { "opacity_image_rotation", "opacityTexTransform.rotation" },
        { "normal_image_scale", "normalTexTransform.scale" },
        { "normal_image_offset", "normalTexTransform.offset" },
        { "normal_image_rotation", "normalTexTransform.rotation" },
        { "specular_roughness_image_scale", "specularRoughnessTexTransform.scale" },
        { "specular_roughness_image_offset", "specularRoughnessTexTransform.offset" },
        { "specular_roughness_image_rotation", "specularRoughnessTexTransform.rotation" },
        { "thin_walled", "thinWalled" }
    };
    // clang-format on
}

shared_ptr<MaterialDefinition> MaterialGenerator::generate(const string& document)
{
    shared_ptr<MaterialDefinition> pDef;

    // If we have already generated this material type, then just used cached material type.
    auto mtliter = _definitions.find(document);
    if (mtliter != _definitions.end())
    {
        pDef = mtliter->second.lock();
        if (pDef)
        {
            return pDef;
        }
        else
        {
            _definitions.erase(document);
        }
    }


    // Create code generator result struct.
    MaterialXCodeGen::BSDFCodeGenerator::Result res;

    // Strings for generated HLSL and entry point.
    string generatedMtlxSetupFunction;

    // The hardcoded inputs are added manually to the generated HLSL and passed to the
    // code-generated set up function.  This is filled in by the inputMapper lambda when the code is
    // generated.
    set<string> hardcodedInputs;

    // Create set of supported BSDF inputs.
    set<string> supportedBSDFInputs;
    for (auto iter : _bsdfInputParamMapping)
    {
        supportedBSDFInputs.insert(iter.first);
    }

    // Run the code generator overriding materialX document name, so that regardless of the name in
    // the document, the generated HLSL is based on a hardcoded name string for caching purposes.
    // NOTE: This will immediate run the code generator and invoke the inputMapper and outputMapper
    // function populate hardcodedInputs and set modifiedNormal.
    if (!_pCodeGenerator->generate(
            document, &res, supportedBSDFInputs, "MaterialXDocument"))
    {
        // Fail if code generation fails.
        // TODO: Proper error handling here.
        AU_ERROR("Failed to generate MaterialX code.");
        return nullptr;
    }

    // Create set of the generated BSDF inputs.
    set<string> bsdfInputs;
    for (int i = 0; i < res.bsdfInputs.size(); i++)
    {
        bsdfInputs.insert(res.bsdfInputs[i].name);
    }

    // Create set of the textures used by the generated setup function.
    map<string, int> textures;
    for (int i = 0; i < res.textures.size(); i++)
    {
        textures[res.textures[i]] = i;
    }

    // Normal modified if the normal BSDF input is active.
    bool modifiedNormal = bsdfInputs.find("normal") != bsdfInputs.end();

    // Create material name from the hash provided by code generator.
    string materialName = "MaterialX_" + Foundation::sHash(res.functionHash);

    // Append the material accessor functions used to read material properties from
    // ByteAddressBuffer.
    UniformBuffer mtlConstantsBuffer(res.materialProperties, res.materialPropertyDefaults);
    generatedMtlxSetupFunction += "struct " + res.materialStructName + "\n";
    generatedMtlxSetupFunction += mtlConstantsBuffer.generateHLSLStruct();
    generatedMtlxSetupFunction += ";\n\n";
    generatedMtlxSetupFunction +=
        mtlConstantsBuffer.generateByteAddressBufferAccessors(res.materialStructName + "_");
    generatedMtlxSetupFunction += "\n";
    generatedMtlxSetupFunction += GLSLToHLSL(res.materialSetupCode);

    // Create a wrapper function that is called by the ray hit entry point to initialize material.
    generatedMtlxSetupFunction += "\nMaterial initializeMaterial"
        "(ShadingData shading, float3x4 objToWorld, out float3 "
        "materialNormal, out bool "
        "isGeneratedNormal) {\n";

    // Fill in the global vertexData struct (used by generated code to access vertex attributes).
    generatedMtlxSetupFunction += "\tvertexData = shading;\n";

    // Fill in the isGeneratedNormal output (set to true if the code-generated setup function
    // generated a normal)
    generatedMtlxSetupFunction += "\tisGeneratedNormal = " + to_string(modifiedNormal) + ";\n";

    // Reset material to default values.
    generatedMtlxSetupFunction += "\tMaterial material = defaultMaterial();\n";

    // Add code to create local texture variables for the textures used by generated code, based on
    // hard-code texture names from Standard Surface.
    // TODO: Data-driven textures that are not mapped to hard coded texture names.
    vector<TextureDefinition> textureVars;
    if (res.textures.size() >= 1)
    {
        // Map first texture to the base_color_image and sampler.
        generatedMtlxSetupFunction +=
            "\tsampler2D base_color_image = createSampler2D(gBaseColorTexture, "
            "gBaseColorSampler);\n";

        // Add to the texture array.
        TextureDefinition txtDef = res.textureDefaults[0];
        txtDef.name              = "base_color_image";
        textureVars.push_back(txtDef);
    }

    if (res.textures.size() >= 2)
    {
        // Map second texture to the opacity_image and sampler.
        generatedMtlxSetupFunction +=
            "\tsampler2D opacity_image = createSampler2D(gOpacityTexture, "
            "gOpacitySampler);\n";

        // Add to the texture array.
        TextureDefinition txtDef = res.textureDefaults[1];
        txtDef.name              = "opacity_image";
        textureVars.push_back(txtDef);
    }

    if (res.textures.size() >= 3)
    {
        // Map second texture to the normal_image and the default sampler.
        generatedMtlxSetupFunction +=
            "\tsampler2D normal_image = createSampler2D(gNormalTexture, "
            "gDefaultSampler);\n";

        // Add to the texture array.
        TextureDefinition txtDef = res.textureDefaults[2];
        txtDef.name              = "normal_image";
        textureVars.push_back(txtDef);
    }

    if (res.textures.size() >= 4)
    {
        // Map second texture to the specular_roughness_image and the default sampler.
        generatedMtlxSetupFunction +=
            "\tsampler2D specular_roughness_image = createSampler2D(gSpecularRoughnessTexture, "
            "gDefaultSampler);\n";

        // Add to the texture array.
        TextureDefinition txtDef = res.textureDefaults[3];
        txtDef.name              = "specular_roughness_image";
        textureVars.push_back(txtDef);
    }

    // Map any additional textures to base color image.
    for (size_t i = textureVars.size(); i < res.textures.size(); i++)
    {
        TextureDefinition txtDef = textureVars[i];
        txtDef.name              = "base_color_image";
        textureVars.push_back(txtDef);
    }

    // Use the define DISTANCE_UNIT which is set in the shader library options.
    generatedMtlxSetupFunction += "\tint distanceUnit = DISTANCE_UNIT;\n";

    // Create temporary material struct
    generatedMtlxSetupFunction += "\t" + res.materialStructName + " setupMaterialStruct;\n";

    // Fill struct using the byte address buffer accessors.
    for (int i = 0; i < res.materialProperties.size(); i++)
    {
        generatedMtlxSetupFunction += "\tsetupMaterialStruct." + res.materialProperties[i].variableName +
            " = " + res.materialStructName + "_" + res.materialProperties[i].variableName +
            "(gMaterialConstants);\n";
        ;
    }

    // Add code to call the generate setup function.
    generatedMtlxSetupFunction += "\t" + res.setupFunctionName + "(\n";

    // First argument is material struct.
    generatedMtlxSetupFunction += "setupMaterialStruct";

    // Add code for all the texture arguments.
    for (int i = 0; i < textureVars.size(); i++)
    {
        // Add texture name.
        generatedMtlxSetupFunction += ",\n";

        // Add texture name.
        generatedMtlxSetupFunction += textureVars[i].name;
    }

    // Add distance unit parameter, if used.
    if (res.hasUnits)
    {
        // Add texture name.
        generatedMtlxSetupFunction += ",\n";

        // Add distance unit variable.
        generatedMtlxSetupFunction += "distanceUnit";
    }

    // Add the BSDF inputs that will be output from setup function.
    for (int i = 0; i < res.bsdfInputs.size(); i++)
    {
        generatedMtlxSetupFunction += ",\n";
        if (res.bsdfInputs[i].name.compare("normal") == 0)
        {
            // Output the generated normal straight into the materialNormal output parameter.
            generatedMtlxSetupFunction += "\t\tmaterialNormal";
        }
        else
        {
            // Output into the material struct to be returned.
            string mappedBSDFInput = _bsdfInputParamMapping[res.bsdfInputs[i].name];
            AU_ASSERT(
                !mappedBSDFInput.empty(), "Invalid BSDF input:%s", res.bsdfInputs[i].name.c_str());
            // Output arguments are written to the material struct.
            generatedMtlxSetupFunction += "\t\tmaterial." + mappedBSDFInput;
        }
    }

    // Finish function call.
    generatedMtlxSetupFunction += ");\n";

    // Replace overwrite metal color with base color.
    // TODO: Metal color is not supported by materialX and not in Standard Surface definition.  So
    // maybe remove it?
    generatedMtlxSetupFunction += "material.metalColor = material.baseColor;";

    // Return the generated material struct.
    generatedMtlxSetupFunction += "\treturn material;\n";

    // Finish setup wrapper function.
    generatedMtlxSetupFunction += "}\n";
    
    // Output the material name
    MaterialTypeSource source(materialName, generatedMtlxSetupFunction);
    MaterialDefaultValues defaults(
        res.materialProperties, res.materialPropertyDefaults, textureVars);

    // Material type is opaque if neither opacity or transmission input is used.
    bool isOpaque = bsdfInputs.find("opacity") == bsdfInputs.end() &&
        bsdfInputs.find("transmission") == bsdfInputs.end();

    // Create update function which just sets opacity flag based on material type opacity.
    function<void(MaterialBase&)> updateFunc = [isOpaque](MaterialBase& mtl) {
        mtl.setIsOpaque(isOpaque);
    };

    pDef = make_shared<MaterialDefinition>(source, defaults, updateFunc);

    _definitions[document] = pDef;

    return pDef;
}


void MaterialGenerator::generateDefinitions(string& definitionHLSLOut)
{

    // Generate the  shared definitions used by all MaterialX material types.
    string definitionGLSL;
    _pCodeGenerator->generateDefinitions(&definitionGLSL);

    // Set the shared definitions;
    definitionHLSLOut = (GLSLToHLSL(definitionGLSL));
}

} // namespace MaterialXCodeGen
} // namespace Aurora
