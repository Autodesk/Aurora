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

#if !defined(DISABLE_UNIT_TESTS)

#include "TestHelpers.h"

#include <filesystem>
#include <gtest/gtest.h>

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

#include "MaterialShader.h"
#include "MaterialX/MaterialGenerator.h"

namespace
{

// Test fixture accessing test asset data path.
class MaterialGeneratorTest : public ::testing::Test
{
public:
    MaterialGeneratorTest() : _dataPath(TestHelpers::kSourceRoot + "/Tests/Assets") {}
    ~MaterialGeneratorTest() {}
    const std::string& dataPath() { return _dataPath; }
    // Test for the existence of the ADSK materialX libraries (in the working folder for the tests)
    bool adskMaterialXSupport() { return std::filesystem::exists("MaterialX/libraries/adsk"); }

#if defined(__APPLE__)
protected:
    void SetUp() override {
      GTEST_SKIP() << "Skipping all material generator tests on MacOS.";
    }
#endif
    
    std::string _dataPath;
};

bool readTextFile(const string& filename, string& textOut)
{
    ifstream is(filename, ifstream::binary);

    // If file could not be opened, continue.
    if (!is)
        return false;
    is.seekg(0, is.end);
    size_t length = is.tellg();
    is.seekg(0, is.beg);
    textOut.resize(length);
    is.read((char*)&textOut[0], length);

    return true;
}

const char* materialXString0 = R""""(
            <?xml version = "1.0" ?>
            <materialx version = "1.37">
               <material name="SS_Material">
                 <shaderref name="SS_ShaderRef1" node="standard_surface">
                  <bindinput name="base_color" type="color3" value="1,0,1" />
                  <bindinput name="specular_color" type="color3" value="0,1,0" />
                  <bindinput name="specular_roughness" type="float" value="0.8" />
                  <bindinput name="specular_IOR" type="float" value = "0.75" />
                  <bindinput name="emission_color" type="color3" value = "0.2,0.2,0.1" />
                  <bindinput name="coat" type="float" value = "0.75" />
                  <bindinput name="coat_roughness" type="float" value = "0.75" />
                </shaderref>
              </material>
            </materialx>
    )"""";

const char* materialXString1 = R""""(
            <?xml version = "1.0" ?>
            <materialx version = "1.37">
               <material name="SS_Material1">
                 <shaderref name="SS_ShaderRef1" node="standard_surface">
                  <bindinput name="base_color" type="color3" value="1,1,1" />
                  <bindinput name="specular_color" type="color3" value="1,1,1" />
                  <bindinput name="specular_roughness" type="float" value="0.1" />
                  <bindinput name="specular_IOR" type="float" value = "0.01" />
                  <bindinput name="emission_color" type="color3" value = "1.0,0.2,0.1" />
                  <bindinput name="coat" type="float" value = "0.75" />
                  <bindinput name="coat_roughness" type="float" value = "0.75" />
                </shaderref>
              </material>
            </materialx>
    )"""";

const char* materialXString2 = R""""(

    <?xml version = "1.0" ?>
    <materialx version = "1.38">
        <nodegraph name="NG1">
        <image name="base_color_image" type="color3">
            <parameter name="file" type="filename" value="../Textures/CoatOfArms.bmp" />
            <parameter name="uaddressmode" type="string" value="periodic" />
            <parameter name="vaddressmode" type="string" value="periodic" />
        </image>
        <output name="out1" type="color3" nodename="base_color_image" />
        </nodegraph>
        <nodegraph name="NG2">
        <image name="specular_roughness_image" type="float">
            <parameter name="file" type="filename" value="../Textures/fishscale_roughness.png" />
            <parameter name="uaddressmode" type="string" value="periodic" />
            <parameter name="vaddressmode" type="string" value="periodic" />
        </image>
        <output name="out1" type="float" nodename="specular_roughness_image" />
        </nodegraph>
        <standard_surface name="SS_Material" type="surfaceshader">
            <input name="base_color" type="color3" nodegraph="NG1" output="out1" />
            <input name="specular_roughness" type="float" nodegraph="NG2" output="out1" />
            <input name="specular_IOR" type="float" value="0.6" />
            <input name="emission_color" type="color3" value="0.0,0.0,0.0" />
            <input name="transmission" type="float" value="0.0" />
            <input name="coat" type="float" value="0.0" />
            <input name="coat_roughness" type="float" value="0.0" />
            <input name="metalness" type="float" value="0.4" />
        </standard_surface>
        <surfacematerial name="SS_Material_shader" type="material">
        <input name="surfaceshader" type="surfaceshader" nodename="SS_Material" />
        </surfacematerial>
        <look name="default">
        <materialassign name="material_assign_default" material="SS_Material_shader" geom="*" />
        </look>
    </materialx>
)"""";

const char* materialXString3 = R""""(

    <?xml version = "1.0" ?>
    <materialx version = "1.38">
        <nodegraph name="NG1">
        <image name="base_color_image" type="color3">
            <parameter name="file" type="filename" value="../Textures/CoatOfArms.bmp" />
        </image>
        <output name="out1" type="color3" nodename="base_color_image" />
        </nodegraph>
        <standard_surface name="SS_Material" type="surfaceshader">
            <input name="base_color" type="color3" nodegraph="NG1" output="out1" />
        </standard_surface>
        <surfacematerial name="SS_Material_shader" type="material">
        <input name="surfaceshader" type="surfaceshader" nodename="SS_Material" />
        </surfacematerial>
        <look name="default">
        <materialassign name="material_assign_default" material="SS_Material_shader" geom="*" />
        </look>
    </materialx>
)"""";

// Basic material generator test.
TEST_F(MaterialGeneratorTest, BasicTest)
{
    string mtlxFolder = Foundation::getModulePath() + "MaterialX";
    Aurora::MaterialXCodeGen::MaterialGenerator matGen(mtlxFolder);

    Aurora::MaterialDefinitionPtr pMtlDef0 = matGen.generate(materialXString0);
    ASSERT_NE(pMtlDef0, nullptr);
    EXPECT_STREQ(pMtlDef0->source().uniqueId.c_str(), "MaterialX_d183faa1b8cb18d7");
    EXPECT_EQ(pMtlDef0->defaults().properties.size(), 7);
    EXPECT_EQ(pMtlDef0->defaults().propertyDefinitions.size(), 7);
    EXPECT_NEAR(pMtlDef0->defaults().properties[2].asFloat(), 0.8f, 0.01f);
    EXPECT_EQ(pMtlDef0->defaults().textureNames.size(), 0);
    EXPECT_EQ(pMtlDef0->defaults().textures.size(), 0);

    Aurora::MaterialDefinitionPtr pMtlDef1 = matGen.generate(materialXString1);
    ASSERT_NE(pMtlDef1, nullptr);
    EXPECT_STREQ(pMtlDef1->source().uniqueId.c_str(), "MaterialX_d183faa1b8cb18d7");
    EXPECT_EQ(pMtlDef1->defaults().properties.size(), 7);
    EXPECT_EQ(pMtlDef1->defaults().propertyDefinitions.size(), 7);
    EXPECT_NEAR(pMtlDef1->defaults().properties[2].asFloat(), 0.1f, 0.01f);
    EXPECT_EQ(pMtlDef1->defaults().textureNames.size(), 0);
    EXPECT_EQ(pMtlDef1->defaults().textures.size(), 0);

    Aurora::MaterialShaderDefinition shaderDef0;
    pMtlDef0->getShaderDefinition(shaderDef0);

    Aurora::MaterialShaderDefinition shaderDef1;
    pMtlDef1->getShaderDefinition(shaderDef1);
    EXPECT_TRUE(shaderDef0.compare(shaderDef1));

    Aurora::MaterialDefinitionPtr pMtlDef2 = matGen.generate(materialXString2);
    ASSERT_NE(pMtlDef2, nullptr);
    EXPECT_STREQ(pMtlDef2->source().uniqueId.c_str(), "MaterialX_845875d7ffe5edaf");
    EXPECT_EQ(pMtlDef2->defaults().properties.size(), 6);
    EXPECT_EQ(pMtlDef2->defaults().propertyDefinitions.size(), 6);
    EXPECT_EQ(pMtlDef2->defaults().textureNames.size(), 2);
    EXPECT_EQ(pMtlDef2->defaults().textures.size(), 2);
    EXPECT_STREQ(pMtlDef2->defaults().textureNames[0].image.c_str(), "base_color_image");
    EXPECT_STREQ(pMtlDef2->defaults().textureNames[0].sampler.c_str(), "base_color_image_sampler");
    EXPECT_STREQ(
        pMtlDef2->defaults().textures[0].defaultFilename.c_str(), "../Textures/CoatOfArms.bmp");
    EXPECT_STREQ(pMtlDef2->defaults().textureNames[1].image.c_str(), "specular_roughness_image");
    EXPECT_STREQ(
        pMtlDef2->defaults().textureNames[1].sampler.c_str(), "specular_roughness_image_sampler");
    EXPECT_STREQ(pMtlDef2->defaults().textures[1].defaultFilename.c_str(),
        "../Textures/fishscale_roughness.png");

    Aurora::MaterialShaderDefinition shaderDef2;
    pMtlDef2->getShaderDefinition(shaderDef2);
    EXPECT_FALSE(shaderDef0.compare(shaderDef2));

    Aurora::MaterialDefinitionPtr pMtlDef2Dupe = matGen.generate(materialXString2);
    Aurora::MaterialDefinition* pMtlDef2Raw    = pMtlDef2.get();
    EXPECT_EQ(pMtlDef2Dupe.get(), pMtlDef2Raw);
    EXPECT_EQ(pMtlDef2.use_count(), 2);
    pMtlDef2Dupe.reset();
    EXPECT_EQ(pMtlDef2.use_count(), 1);
    pMtlDef2.reset();
}

TEST_F(MaterialGeneratorTest, MaterialShaderLibraryTest)
{
    string mtlxFolder = Foundation::getModulePath() + "MaterialX";
    Aurora::MaterialXCodeGen::MaterialGenerator matGen(mtlxFolder);

    Aurora::MaterialShaderLibrary shaderLib({ "foo", "bar" });

    Aurora::MaterialDefinitionPtr pMtlDef0 = matGen.generate(materialXString0);
    Aurora::MaterialShaderDefinition shaderDef0;
    pMtlDef0->getShaderDefinition(shaderDef0);

    Aurora::MaterialShaderPtr pShader0 = shaderLib.acquire(shaderDef0);
    ASSERT_NE(pShader0, nullptr);
    EXPECT_EQ(pShader0->libraryIndex(), 0);
    EXPECT_EQ(pShader0->entryPoints().size(), 2);

    Aurora::MaterialDefinitionPtr pMtlDef1 = matGen.generate(materialXString1);
    Aurora::MaterialShaderDefinition shaderDef1;
    pMtlDef1->getShaderDefinition(shaderDef1);

    Aurora::MaterialShaderPtr pShader1 = shaderLib.acquire(shaderDef1);
    ASSERT_NE(pShader1, nullptr);
    EXPECT_EQ(pShader1.get(), pShader0.get());

    Aurora::MaterialDefinitionPtr pMtlDef2 = matGen.generate(materialXString2);
    Aurora::MaterialShaderDefinition shaderDef2;
    pMtlDef2->getShaderDefinition(shaderDef2);

    Aurora::MaterialShaderPtr pShader2 = shaderLib.acquire(shaderDef2);
    ASSERT_NE(pShader2, nullptr);
    EXPECT_EQ(pShader2->libraryIndex(), 1);
    EXPECT_NE(pShader2.get(), pShader0.get());

    int numCompiled  = 0;
    int numDestroyed = 0;
    auto compileFunc = [&numCompiled](const Aurora::MaterialShader&) {
        numCompiled++;
        return true;
    };
    auto destroyFunc = [&numDestroyed](int) { numDestroyed++; };

    shaderLib.update(compileFunc, destroyFunc);
    EXPECT_EQ(numCompiled, 2);
    EXPECT_EQ(numDestroyed, 0);

    pShader2.reset();
    shaderLib.update(compileFunc, destroyFunc);
    EXPECT_EQ(numCompiled, 2);
    EXPECT_EQ(numDestroyed, 1);

    pShader0->incrementRefCount("foo");
    shaderLib.update(compileFunc, destroyFunc);
    EXPECT_EQ(numCompiled, 3);
    EXPECT_EQ(numDestroyed, 1);

    Aurora::MaterialShaderPtr pShader3 = shaderLib.acquire(shaderDef2);
    ASSERT_NE(pShader3, nullptr);
    EXPECT_EQ(pShader3->libraryIndex(), 1);

    shaderLib.update(compileFunc, destroyFunc);
    EXPECT_EQ(numCompiled, 4);
    EXPECT_EQ(numDestroyed, 1);
}

TEST_F(MaterialGeneratorTest, CodeGenTest)
{
    string mtlxFolder = Foundation::getModulePath() + "MaterialX";
    Aurora::MaterialXCodeGen::BSDFCodeGenerator codeGen(mtlxFolder);

    map<string, string> bsdfInputParamMapping = { { "base", "base" }, { "base_color", "baseColor" },
        { "coat", "coat" }, { "coat_color", "coatColor" }, { "coat_roughness", "coatRoughness" },
        { "coat_anisotropy", "coatAnisotropy" }, { "coat_rotation", "coatRotation" },
        { "coat_IOR", "coatIOR" }, { "diffuse_roughness", "diffuseRoughness" },
        { "metalness", "metalness" }, { "specular", "specular" },
        { "specular_color", "specularColor" }, { "specular_roughness", "specularRoughness" },
        { "specular_anisotropy", "specularAnisotropy" }, { "specular_IOR", "specularIOR" },
        { "specular_rotation", "specularRotation" }, { "transmission", "transmission" },
        { "transmission_color", "transmissionColor" }, { "subsurface", "subsurface" },
        { "subsurfaceColor", "subsurfaceColor" }, { "subsurface_scale", "subsurfaceScale" },
        { "subsurface_radius", "subsurfaceRadius" }, { "sheen", "sheen" },
        { "sheen_color", "sheenColor" }, { "sheen_roughness", "sheenRoughness" },
        { "coat", "coat" }, { "coat_color", "coatColor" }, { "coat_roughness", "coatRoughness" },
        { "coat_anisotropy", "coatAnisotropy" }, { "coat_rotation", "coatRotation" },
        { "coat_IOR", "coatIOR" }, { "coat_affect_color", "coatAffectColor" },
        { "coat_affect_roughness", "coatAffectRoughness" }, { "emission", "emission" },
        { "emission_color", "emissionColor" }, { "opacity", "opacity" },
        { "base_color_image_scale", "baseColorTexTransform.scale" },
        { "base_color_image_offset", "baseColorTexTransform.offset" },
        { "base_color_image_rotation", "baseColorTexTransform.rotation" },
        { "emission_color_image_scale", "emissionColorTexTransform.scale" },
        { "emission_color_image_offset", "emissionColorTexTransform.offset" },
        { "emission_color_image_rotation", "emissionColorTexTransform.rotation" },
        { "opacity_image_scale", "opacityTexTransform.scale" },
        { "opacity_image_offset", "opacityTexTransform.offset" },
        { "opacity_image_rotation", "opacityTexTransform.rotation" },
        { "normal_image_scale", "normalTexTransform.scale" },
        { "normal_image_offset", "normalTexTransform.offset" },
        { "normal_image_rotation", "normalTexTransform.rotation" },
        { "specular_roughness_image_scale", "specularRoughnessTexTransform.scale" },
        { "specular_roughness_image_offset", "specularRoughnessTexTransform.offset" },
        { "specular_roughness_image_rotation", "specularRoughnessTexTransform.rotation" },
        { "thin_walled", "thinWalled" }, { "normal", "normal" } };

    // Create set of supported BSDF inputs.
    set<string> supportedBSDFInputs;
    for (auto iter : bsdfInputParamMapping)
    {
        supportedBSDFInputs.insert(iter.first);
    }

    Aurora::MaterialXCodeGen::BSDFCodeGenerator::Result res;
    bool ok;
    string errMsg, defStr;

    ok = codeGen.generate(materialXString0, &res, supportedBSDFInputs);
    ASSERT_TRUE(ok);

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX0_Setup.glsl",
        res.materialSetupCode, "Generated setup code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX0_Struct.glsl",
        res.materialStructCode, "Generated struct code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.generateDefinitions(&defStr);
    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX0_Defs.glsl",
        defStr, "Generated definitions code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.clearDefinitions();
    ok = codeGen.generate(materialXString1, &res, supportedBSDFInputs);
    ASSERT_TRUE(ok);

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX1_Setup.glsl",
        res.materialSetupCode, "Generated setup code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX1_Struct.glsl",
        res.materialStructCode, "Generated struct code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.generateDefinitions(&defStr);
    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX1_Defs.glsl",
        defStr, "Generated definitions code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.clearDefinitions();
    ok = codeGen.generate(materialXString2, &res, supportedBSDFInputs);
    ASSERT_TRUE(ok);

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX2_Setup.glsl",
        res.materialSetupCode, "Generated setup code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX2_Struct.glsl",
        res.materialStructCode, "Generated struct code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.generateDefinitions(&defStr);
    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX2_Defs.glsl",
        defStr, "Generated definitions code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.clearDefinitions();
    ok = codeGen.generate(materialXString3, &res, supportedBSDFInputs);
    ASSERT_TRUE(ok);

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX3_Setup.glsl",
        res.materialSetupCode, "Generated setup code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX3_Struct.glsl",
        res.materialStructCode, "Generated struct code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.generateDefinitions(&defStr);
    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/TestMaterialX3_Defs.glsl",
        defStr, "Generated definitions code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    string readMtlXText;

    if (adskMaterialXSupport())
    {
        ok = readTextFile(dataPath() + "/Materials/Decals/test_decal_mask.mtlx", readMtlXText);
        ASSERT_TRUE(ok);

        ok = codeGen.generate(readMtlXText, &res, supportedBSDFInputs);
        ASSERT_TRUE(ok);

        errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/Mask_Setup.glsl",
            res.materialSetupCode, "Generated setup code comparison failed");
        EXPECT_TRUE(errMsg.empty()) << errMsg;

        errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/Mask_Struct.glsl",
            res.materialStructCode, "Generated struct code comparison failed");
        EXPECT_TRUE(errMsg.empty()) << errMsg;

        codeGen.generateDefinitions(&defStr);
        errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/Mask_Defs.glsl", defStr,
            "Generated definitions code comparison failed");
        EXPECT_TRUE(errMsg.empty()) << errMsg;
    }

    ok = readTextFile(dataPath() + "/Materials/HdAuroraTextureTest.mtlx", readMtlXText);
    ASSERT_TRUE(ok);

    ok = codeGen.generate(readMtlXText, &res, supportedBSDFInputs);
    ASSERT_TRUE(ok);

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/HdAuroraTextureTest_Setup.glsl",
        res.materialSetupCode, "Generated setup code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/HdAuroraTextureTest_Struct.glsl",
        res.materialStructCode, "Generated struct code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;

    codeGen.generateDefinitions(&defStr);
    errMsg = TestHelpers::compareTextFile(dataPath() + "/TextFiles/HdAuroraTextureTest_Defs.glsl",
        defStr, "Generated definitions code comparison failed");
    EXPECT_TRUE(errMsg.empty()) << errMsg;
}

} // namespace

#endif
