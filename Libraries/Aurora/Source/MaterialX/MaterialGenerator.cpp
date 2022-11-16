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
    _materialXOutputParamMapping =
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

bool MaterialGenerator::generate(
    const string& document, map<string, Value>* pDefaultValuesOut, MaterialTypeSource& sourceOut)
{
    // Create code generator result struct.
    MaterialXCodeGen::BSDFCodeGenerator::Result res;

    // The hardcoded inputs are added manually to the generated HLSL and passed to the
    // code-generated set up function.  This is filled in by the inputMapper lamdba when the code is
    // generated.
    set<string> hardcodedInputs;

    // Callback function to map MaterialX inputs to Aurora uniforms and textures.
    // This function will be called for each input parameter in the MaterialX document that is
    // connected to a mapped output. Each "top level" input parameter will be prefixed with the top
    // level shader name for the MaterialX document, other input parameters will be named for the
    // node graph they are part of (e.g. generic_diffuse)
    MaterialXCodeGen::BSDFCodeGenerator::ParameterMappingFunction inputMapper =
        [&](const string& name, Aurora::IValues::Type type, const string& topLevelShaderName) {
            // Top-level inputs are prefixed with the top level material name.
            if (name.size() > topLevelShaderName.size())
            {
                // Strip the top level material name (and following underscore) from the input name.
                string nameStripped = name.substr(topLevelShaderName.size() + 1);

                // If this is one of the standard surface inputs then map to that.
                auto iter = _materialXOutputParamMapping.find(nameStripped);
                if (iter != _materialXOutputParamMapping.end())
                    return iter->second;
            }

            // By default return an empty string (which will leave input unmapped.)
            string mappedName = "";

            // Map anything containing the string 'unit_unit_to' to the distance unit.
            if (name.find("unit_unit_to") != string::npos)
            {
                mappedName = "distanceUnit";
            }

            // Map any image inputs using a heuristic that should guess the right texture mapping
            // for any textures we care about.
            if (type == Aurora::IValues::Type::Image)
            {
                mappedName = mapMaterialXTextureName(name);
                if (mappedName.empty())
                {
                    // Leaving a texture unassigned will result in a HLSL compilation error, so map
                    // unrecognized textures to gBaseColorTexture and print a warning. This avoids
                    // the shader compile error, but will render incorrectly.
                    AU_WARN("Unrecognized texture variable %s, setting to gBaseColorTexture.",
                        name.c_str());
                    mappedName = "base_color_image";
                }
            }

            // Map any int parameters as sampler properties.
            if (type == Aurora::IValues::Type::Int)
            {
                string mappedTextureName = mapMaterialXTextureName(name);

                // Any int input that can be mapped to hardcoded texture name is treated as address
                // mode.
                // TODO: Only base color and opacity image currently supports samplers.
                if (!mappedTextureName.empty() &&
                    (mappedTextureName.compare("base_color_image") == 0 ||
                        mappedTextureName.compare("opacity_image") == 0))
                {
                    // Map uaddressmode to the image's AddressModeU property.
                    if (name.find("uaddressmode") != string::npos)
                        mappedName = mappedTextureName + "_sampler_uaddressmode";
                    // Map uaddressmode to the image's AddressModeV property.
                    else if (name.find("vaddressmode") != string::npos)
                        mappedName = mappedTextureName + "_sampler_vaddressmode";
                }
            }
            if (type == Aurora::IValues::Type::Float2)
            {
                string mappedTextureName = mapMaterialXTextureName(name);

                // Any vec2 input that can be mapped to hardcoded texture name is treated as texture
                // transform property.
                if (!mappedTextureName.empty())
                {
                    // Map uv_offset to the image transform's offset property.
                    if (name.find("uv_offset") != string::npos)
                        mappedName = mappedTextureName + "_offset";
                    // Map uv_scale to the image transform's scale property.
                    else if (name.find("uv_scale") != string::npos)
                        mappedName = mappedTextureName + "_scale";
                }
            }
            if (type == Aurora::IValues::Type::Float)
            {
                string mappedTextureName = mapMaterialXTextureName(name);

                // Any float input that can be mapped to hardcoded texture name as texture rotation.
                if (!mappedTextureName.empty())
                {
                    // Map uv_scale to the image transform's rotaiton property.
                    if (name.find("rotation_angle") != string::npos)
                        mappedName = mappedTextureName + "_rotation";
                }
            }

            //  Add to the hardcoded inputs.
            if (!mappedName.empty())
                hardcodedInputs.insert(mappedName);

            // Return the mapped name
            return mappedName;
        };

    // Set to true by lambda if the normal has been modified in code-generated material setup.
    bool modifiedNormal = false;

    // Callback function to map MaterialX setup outputs to the Aurora material parameter.
    MaterialXCodeGen::BSDFCodeGenerator::ParameterMappingFunction outputMapper =
        [&](const string& name, Aurora::IValues::Type /*type*/,
            const string& /*topLevelShaderName*/) {
            // If there is direct mapping to aurora material use that.
            auto iter = _materialXOutputParamMapping.find(name);
            if (iter != _materialXOutputParamMapping.end())
                return iter->second;

            // If the setup code has generated a normal, set the modified flag.
            if (name.compare("normal") == 0)
            {
                modifiedNormal = true;
                return name;
            }

            // Otherwise don't code generate this output.
            return string("");
        };

    // Remap the parameters *back* to the materialX name, as the material inputs are in snake case,
    // not camel case.
    // TODO: We should unify camel-vs-snake case for material parameters.
    map<string, string> remappedInputs;
    for (auto iter : _materialXOutputParamMapping)
    {
        remappedInputs[iter.second] = iter.first;
    }

    // Run the code generator overriding materialX document name, so that regardless of the name in
    // the document, the generated HLSL is based on a hardcoded name string for caching purposes.
    // NOTE: This will immediate run the code generator and invoke the inputMapper and outputMapper
    // function populate hardcodedInputs and set modifiedNormal.
    if (!_pCodeGenerator->generate(document, &res, inputMapper, outputMapper, "MaterialXDocument"))
    {
        // Fail if code generation fails.
        // TODO: Proper error handling here.
        AU_ERROR("Failed to generate MaterialX code.");
        return false;
    }

    // Map of samplers to collate sampler properties.
    map<string, Properties> samplerProperties;

    // Get the default values.
    pDefaultValuesOut->clear();
    for (int i = 0; i < res.argumentsUsed.size(); i++)
    {
        auto arg = res.argumentsUsed[i];
        // Code generator returns input and output values, we only care about inputs.
        if (!arg.isOutput)
        {
            // For all standard surface inputs, remap back to snake case.
            string remappedName = arg.name;

            // We need to collate the seperate sampler properties (vaddressmode or uaddressmode)
            // into sampler object.
            bool isSamplerParam = false;
            if (arg.name.find("uaddressmode") != string::npos ||
                arg.name.find("vaddressmode") != string::npos)
            {
                isSamplerParam = true;

                // Compute sampler name from property name.
                string samplerName = arg.name;
                samplerName.erase(samplerName.length() - string("_uaddressmode").length());

                // If no sampler properties, create one.
                if (samplerProperties.find(samplerName) == samplerProperties.end())
                {
                    samplerProperties[samplerName] = {};
                }

                // Map int value to address mode.
                // These int values are abrirarily defined in MaterialX source:
                // source\MaterialXRender\ImageHandler.h
                // TODO: Make this less error prone.
                string addressMode = Names::AddressModes::kWrap;
                switch (arg.pDefaultValue->asInt())
                {
                case 1:
                    addressMode = Names::AddressModes::kClamp;
                    break;
                case 2:
                    addressMode = Names::AddressModes::kWrap;
                    break;
                case 3:
                    addressMode = Names::AddressModes::kMirror;
                    break;
                default:
                    break;
                }

                // Set the sampler properties for U/V address mode.
                if (arg.name.find("uaddressmode") != string::npos)
                {
                    samplerProperties[samplerName][Names::SamplerProperties::kAddressModeU] =
                        addressMode;
                }
                else if (arg.name.find("vaddressmode") != string::npos)
                {
                    samplerProperties[samplerName][Names::SamplerProperties::kAddressModeV] =
                        addressMode;
                }
            }

            // Don't map individual sampler parameters to default values, as they are added as
            // sampler objects below.
            if (!isSamplerParam)
            {
                // Remap if needed.
                auto hardcodedIter = hardcodedInputs.find(arg.name);
                if (hardcodedIter != hardcodedInputs.end())
                {
                    // If this is a hard-oded input, use the hardcoded name directly.
                    remappedName = arg.name;
                }
                else
                {
                    // For standard surface inputs, remap back to snake case.
                    remappedName = remappedInputs[arg.name];
                }

                // Add the default value to the output map.
                pDefaultValuesOut->insert(make_pair(remappedName, *arg.pDefaultValue));
            }
        }
    }

    // Create sampler objects for the sampler properties.
    // TODO: Cache these to avoid unnessacary sampler objects.
    for (auto iter = samplerProperties.begin(); iter != samplerProperties.end(); iter++)
    {
        pDefaultValuesOut->insert(
            make_pair(iter->first, _pRenderer->createSamplerPointer(iter->second)));
    }

    // Create material name from the hash provided by code generator.
    string materialName = "MaterialX_" + Foundation::sHash(res.functionHash);

    // Begin with the setup code (converted to HLSL)
    string generatedMtlxSetupFunction = GLSLToHLSL(res.materialSetupCode);

    // Create a wrapper function that is called by the ray hit entry point to initialize material.
    generatedMtlxSetupFunction +=
        "\nMaterial initializeMaterial(ShadingData shading, float3x4 objToWorld, out float3 "
        "materialNormal, out bool "
        "isGeneratedNormal) {\n";

    // Fill in the global vertexData struct (used by generated code to access vertex attributes).
    generatedMtlxSetupFunction += "\tvertexData = shading;\n";

    // Fill in the isGeneratedNormal output (set to true if the code-generated setup function
    // generated a normal)
    generatedMtlxSetupFunction += "\tisGeneratedNormal = " + to_string(modifiedNormal) + ";\n";

    // The hardcoded initializeDefaultMaterial() function requires a normal parameter, this should
    // never be used here.
    generatedMtlxSetupFunction +=
        "\nfloat3 unusedObjectSpaceNormal = shading.normal; bool unusedIsGeneratedNormal;\n";

    // Add code to call default initialize material function.
    // NOTE: Most of this will get overwritten, but the compiler should be clever enough to know
    // that and remove dead code.
    generatedMtlxSetupFunction +=
        "\tMaterial material = initializeDefaultMaterial(shading, ObjectToWorld3x4(), "
        "unusedObjectSpaceNormal, "
        "unusedIsGeneratedNormal);\n";

    // MaterialX accepts addresmode parameters as dummy integers that are passed down to the shader
    // code, but then never used. So we need to define them as arbritary ints to avoid compile
    // errors.
    // TODO: Less hacky way to do this.
    if (hardcodedInputs.find("base_color_image_sampler_uaddressmode") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction += "\tint base_color_image_sampler_uaddressmode = 0;\n";
    }
    if (hardcodedInputs.find("base_color_image_sampler_vaddressmode") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction += "\tint base_color_image_sampler_vaddressmode = 0;\n";
    }
    if (hardcodedInputs.find("opacity_image_sampler_uaddressmode") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction += "\tint opacity_image_sampler_uaddressmode = 0;\n";
    }
    if (hardcodedInputs.find("opacity_image_sampler_vaddressmode") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction += "\tint opacity_image_sampler_vaddressmode = 0;\n";
    }

    // Add code to create local texture variables for the textures used by generated code.
    // TODO: Add more and be more rigorous about finding which textures are used.
    if (hardcodedInputs.find("base_color_image") != hardcodedInputs.end())
    {
        // Ensure the correct sampler is used for the base color sampler (not default sampler).
        generatedMtlxSetupFunction +=
            "\tsampler2D base_color_image = createSampler2D(gBaseColorTexture, "
            "gBaseColorSampler);\n";
    }
    if (hardcodedInputs.find("specular_roughness_image") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction +=
            "\tsampler2D specular_roughness_image = createSampler2D(gSpecularRoughnessTexture, "
            "gDefaultSampler);\n";
    }
    if (hardcodedInputs.find("normal_image") != hardcodedInputs.end())
    {
        generatedMtlxSetupFunction +=
            "\tsampler2D normal_image = createSampler2D(gNormalTexture, "
            "gDefaultSampler);\n";
    }
    if (hardcodedInputs.find("opacity_image") != hardcodedInputs.end())
    {
        // Ensure the correct sampler is used for the opacity sampler (not default sampler).
        generatedMtlxSetupFunction +=
            "\tsampler2D opacity_image = createSampler2D(gOpacityTexture, "
            "gOpacitySampler);\n";
    }

    // Use the define DISTANCE_UNIT which is set in the shader library options.
    generatedMtlxSetupFunction += "\tint distanceUnit = DISTANCE_UNIT;\n";

    // Add code to call the generate setup function.
    generatedMtlxSetupFunction += "\t" + res.setupFunctionName + "(\n";

    // Add code for all the required arguments.
    for (int i = 0; i < res.argumentsUsed.size(); i++)
    {
        // Current argument.
        auto arg = res.argumentsUsed[i];

        // Append comma if needed.
        if (i > 0)
            generatedMtlxSetupFunction += ",\n";

        if (arg.isOutput)
        {
            if (arg.name.compare("normal") == 0)
            {
                // Output the generated normal straight into the materialNormal output parameter.
                generatedMtlxSetupFunction += "\t\tmaterialNormal";
            }
            else
            {
                // Output arguments are written to the material struct.
                generatedMtlxSetupFunction += "\t\tmaterial." + arg.name;
            }
        }
        else
        {
            if (hardcodedInputs.find(arg.name) != hardcodedInputs.end())
            {
                if (_materialXOutputParamMapping.find(arg.name) !=
                    _materialXOutputParamMapping.end())
                {
                    generatedMtlxSetupFunction +=
                        "\t\tgMaterialConstants." + _materialXOutputParamMapping[arg.name];
                }
                else
                {
                    // Hardcoded inputs are specified in the HLSL above and passed in directly.
                    generatedMtlxSetupFunction += "\t\t" + arg.name;
                }
            }
            else
            {
                // Other arguments are read from constant buffer transfered from CPU.
                generatedMtlxSetupFunction += "\t\tgMaterialConstants." + arg.name;
            }
        }
    }

    // Finish function call.
    generatedMtlxSetupFunction += ");\n";

    // Return the generated material struct.
    generatedMtlxSetupFunction += "\treturn material;\n";

    // Finish setup wrapper function.
    generatedMtlxSetupFunction += "}\n";

    // Output the material name
    sourceOut.bsdf  = "";
    sourceOut.setup = generatedMtlxSetupFunction;
    sourceOut.name  = materialName;

    return true;
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
