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
/**
 * Copyright 2021 Autodesk, Inc.
 * All rights reserved.
 *
 * This computer source code and related instructions and comments are the unpublished confidential
 * and proprietary information of Autodesk, Inc. and are protected under Federal copyright and state
 * trade secret law. They may not be disclosed to, copied or used by any third party without the
 * prior written consent of Autodesk, Inc.
 *
 */

#if !defined(DISABLE_UNIT_TESTS)

#include "TestHelpers.h"
#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <streambuf>

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

#include "PathTracing/MaterialX/BSDFCodeGenerator.h"

namespace
{

// Test fixture accessing test asset data path.
class BSDFGeneratorTest : public ::testing::Test
{
public:
    BSDFGeneratorTest() : _dataPath(TestHelpers::kSourceRoot + "/Renderers/Tests/Data") {}
    ~BSDFGeneratorTest() {}
    const std::string& dataPath() { return _dataPath; }

protected:
    std::string _dataPath;
};

// If 1 dump the generated code to files in output folder, for development purposes.
#define DUMP_GENERATED_SHADERS 0

void compareArguments(
    const std::vector<Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator::MaterialArgument>&
        output,
    const std::vector<Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator::MaterialArgument>&
        correct,
    const std::string& message)
{

    ASSERT_EQ(output.size(), correct.size()) << message;
    for (int i = 0; i < output.size(); i++)
    {

        ASSERT_STREQ(output[i].name.c_str(), correct[i].name.c_str())
            << message << " i==" << std::to_string(i);
        ASSERT_EQ(output[i].type, correct[i].type) << message << " i==" << std::to_string(i);
        ASSERT_EQ(output[i].isOutput, correct[i].isOutput)
            << message << " i==" << std::to_string(i);
    }
}

// Basic BSDF generator test.
TEST_F(BSDFGeneratorTest, BasicTest)
{
    // Test MaterialX document.
    std::string testMtl =
        "<?xml version=\"1.0\" ?>\n"
        "<materialx version=\"1.37\">\n"
        "  <nodegraph name=\"NG_Test1\">\n"
        "    <tiledimage name=\"image_color\" type=\"color3\">\n"
        "        <input name=\"file\" type=\"filename\" value=\"brass_color.jpg\" "
        "colorspace=\"srgb_texture\" />\n"
        "        <input name=\"uvtiling\" type=\"vector2\" value=\"1.0, 1.0\" />\n"
        "    </tiledimage>\n"
        "    <position name=\"positionWorld\" type=\"vector3\" />\n"
        "    <constant name=\"constant_1\" type=\"color3\">\n"
        "      <parameter name=\"value\" type=\"color3\" value=\"1.0, 0.1, 0.0\" />\n"
        "    </constant>\n"
        "    <constant name=\"mix_amount\" type=\"float\">\n"
        "      <parameter name=\"value\" type=\"float\" value=\"0.6\" />\n"
        "    </constant>\n"
        "    <mix name=\"mix1\" type=\"color3\">\n"
        "      <input name=\"mix\" type=\"float\" nodename=\"mix_amount\" />\n"
        "      <input name=\"bg\" type=\"color3\" nodename=\"constant_1\" />\n"
        "      <input name=\"fg\" type=\"color3\" nodename=\"image_color\" />\n"
        "    </mix>\n"
        "    <output name=\"out\" type=\"color3\" nodename=\"mix1\" />\n"
        "  </nodegraph>\n"
        "\n"
        "  <material name=\"Test_Material\">\n"
        "    <shaderref name=\"SR_Test1\" node=\"standard_surface\">\n"
        "      <bindinput name=\"base\" type=\"float\" value=\"1.0\" />\n"
        "      <bindinput name=\"base_color\" type=\"color3\" nodegraph=\"NG_Test1\" "
        "output=\"out\" />\n"
        "      <bindinput name=\"specular_color\" type=\"color3\" value=\"1.0, 1.0, 1.0\" />\n"
        "      <bindinput name=\"specular_roughness\" type=\"float\" value=\"0.6\" />\n"
        "    </shaderref>\n"
        "  </material>\n"
        "</materialx>";

    // Create a BSDF code generator for all documents in this test.
    Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator gen;
    Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator::Result res;

    // Local variables, filled in by input mapper lambda.
    string inputsStr = "";
    set<string> inputTopLevelShaderNodeSet;

    // Local variables, filled in by output mapper lambda.
    string outputsStr = "";
    set<string> outputTopLevelShaderNodeSet;

    // Input mapper used to map inputs to the MaterialX generated material code.
    Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator::ParameterMappingFunction inputMapper =
        [&](const string& name, Aurora::Aurora::IValues::Type type,
            const string& topLevelShaderName) {
            // Add all the inputs (and their type) to the inputsStr local variable.
            inputsStr += name + "(" + to_string((int)type) + ") ";

            // Add the top level shader name to a local set variable.
            inputTopLevelShaderNodeSet.insert(topLevelShaderName);

            // Map some input variables to arbitrary names.
            if (name.compare("SR_Test1_diffuse_roughness") == 0)
            {
                return "foo";
            }
            if (name.compare("image_color_realworldtilesize") == 0)
            {
                return "bar";
            }

            return "";
        };

    Aurora::Aurora::MaterialXCodeGen::BSDFCodeGenerator::ParameterMappingFunction outputMapper =
        [&](const string& name, Aurora::Aurora::IValues::Type type,
            const string& topLevelShaderName) {
            // Add all the outputs (and their type) to the inputsStr local variable.
            outputsStr += name + "(" + to_string((int)type) + ") ";

            // Add the top level shader name to a local set.
            outputTopLevelShaderNodeSet.insert(topLevelShaderName);

            // Map some output variables to arbitrary names.
            if (name.compare("base_color") == 0)
            {
                return "bibble";
            }
            if (name.compare("sheen_color") == 0)
            {
                return "bobble";
            }
            return "";
        };

    // Generate code and ensure success (this will invoke both mapper functions and fill in the
    // local variables above.)
    ASSERT_TRUE(gen.generate(testMtl, &res, inputMapper, outputMapper));

    // The same top level shader node name should be passed for all the inputs.
    ASSERT_EQ(inputTopLevelShaderNodeSet.size(), 1);

    // The top level shader node name should be the name of the standard_surface shader node.
    ASSERT_NE(inputTopLevelShaderNodeSet.find("SR_Test1"), inputTopLevelShaderNodeSet.end());

    // Ensure all the inputs from the materialX document are passed to input mapper correctly.
    ASSERT_STREQ(inputsStr.c_str(),
        "image_color_file(6) image_color_default(4) image_color_uvtiling(4) "
        "image_color_uvoffset(4) image_color_realworldimagesize(4) "
        "image_color_realworldtilesize(4) image_color_filtertype(2) image_color_framerange(2) "
        "image_color_frameoffset(2) image_color_frameendaction(2) mix1_bg(4) mix1_mix(3) "
        "SR_Test1_sheen_color(4) ");

    // The same top level shader node name should be passed for all the outputs.
    ASSERT_EQ(outputTopLevelShaderNodeSet.size(), 1);

    // The top level shader node name should be the name of the standard_surface shader node.
    ASSERT_NE(outputTopLevelShaderNodeSet.find("SR_Test1"), outputTopLevelShaderNodeSet.end());

    // Ensure all the outputs from the materialX setup code (which are the inputs to the standard
    // surface node) are passed to output mapper correctly.
    ASSERT_STREQ(outputsStr.c_str(),
        "base(3) base_color(4) diffuse_roughness(3) metalness(3) specular(3) specular_color(4) "
        "specular_roughness(3) specular_IOR(3) specular_anisotropy(3) specular_rotation(3) "
        "transmission(3) transmission_color(4) transmission_depth(3) transmission_scatter(4) "
        "transmission_scatter_anisotropy(3) transmission_dispersion(3) "
        "transmission_extra_roughness(3) subsurface(3) subsurface_color(4) subsurface_radius(4) "
        "subsurface_scale(3) subsurface_anisotropy(3) sheen(3) sheen_color(4) sheen_roughness(3) "
        "coat(3) coat_color(4) coat_roughness(3) coat_anisotropy(3) coat_rotation(3) coat_IOR(3) "
        "coat_normal(4) coat_affect_color(3) coat_affect_roughness(3) thin_film_thickness(3) "
        "thin_film_IOR(3) emission(3) emission_color(4) opacity(4) thin_walled(1) normal(4) "
        "tangent(4) ");

    // Ensure hash has expected value.
    ASSERT_STREQ(res.setupFunctionName.c_str(), "setupMaterial_b9984be493e4afb7");
    ASSERT_EQ(((uint32_t*)&res.functionHash)[1], 0xb9984be4ul);
    ASSERT_EQ(((uint32_t*)&res.functionHash)[0], 0x93e4afb7ul);

    // Ensure generated code length non-zero.
    // TODO: Save to disk and do proper snapshot comparison.
    ASSERT_GT(res.materialSetupCode.size(), 0);

    // Ensure expected function arguments.
    // NOTE: the "foo" input argument is never generated as the "diffuse_roughness" output is not
    // mapped.
    compareArguments(res.argumentsUsed,
        {
            { "bar", Aurora::Aurora::IValues::Type::Float3, false },
            { "bibble", Aurora::Aurora::IValues::Type::Float3, true },
            { "bobble", Aurora::Aurora::IValues::Type::Float3, true },
        },
        "Argument Comparison " + std::string(__FILE__) + " (" + std::to_string(__LINE__) + ")");

    // Get the definitions for the generated document.
    std::string definitions;
    int numDefinitions = gen.generateDefinitions(&definitions);

    // Ensure expected number of definitions.
    ASSERT_EQ(numDefinitions, 1);

    // Ensure generated code length non-zero.
    // TODO: Save to disk and do proper snapshot comparison.
    ASSERT_GT(definitions.size(), 0);

    // Generate code with overriden document name.
    ASSERT_TRUE(gen.generate(testMtl, &res, inputMapper, outputMapper, "FooBarToot"));
    ASSERT_EQ(res.materialSetupCode.find("NG_Test1"), string::npos);

    // The previous call should have cleared the definitions
    gen.clearDefinitions();
    numDefinitions = gen.generateDefinitions(&definitions);
    ASSERT_EQ(numDefinitions, 0);
    ASSERT_EQ(definitions.size(), 0);
}

} // namespace

#endif
