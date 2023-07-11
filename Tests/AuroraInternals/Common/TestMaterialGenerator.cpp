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

protected:
    std::string _dataPath;
};
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

// Basic material generator test.
TEST_F(MaterialGeneratorTest, BasicTest)
{
    string mtlxFolder = Foundation::getModulePath() + "MaterialX";
    Aurora::MaterialXCodeGen::MaterialGenerator matGen(mtlxFolder);

    Aurora::MaterialDefinitionPtr pMtlDef0 = matGen.generate(materialXString0);
    ASSERT_NE(pMtlDef0, nullptr);
    ASSERT_STREQ(pMtlDef0->source().uniqueId.c_str(), "MaterialX_d183faa1b8cb18d7");
    ASSERT_EQ(pMtlDef0->defaults().properties.size(), 7);
    ASSERT_EQ(pMtlDef0->defaults().propertyDefinitions.size(), 7);
    ASSERT_NEAR(pMtlDef0->defaults().properties[2].asFloat(), 0.8f, 0.01f);
    ASSERT_EQ(pMtlDef0->defaults().textureNames.size(), 0);
    ASSERT_EQ(pMtlDef0->defaults().textures.size(), 0);

    Aurora::MaterialDefinitionPtr pMtlDef1 = matGen.generate(materialXString1);
    ASSERT_NE(pMtlDef1, nullptr);
    ASSERT_STREQ(pMtlDef1->source().uniqueId.c_str(), "MaterialX_d183faa1b8cb18d7");
    ASSERT_EQ(pMtlDef1->defaults().properties.size(), 7);
    ASSERT_EQ(pMtlDef1->defaults().propertyDefinitions.size(), 7);
    ASSERT_NEAR(pMtlDef1->defaults().properties[2].asFloat(), 0.1f, 0.01f);
    ASSERT_EQ(pMtlDef1->defaults().textureNames.size(), 0);
    ASSERT_EQ(pMtlDef1->defaults().textures.size(), 0);

    Aurora::MaterialShaderDefinition shaderDef0;
    pMtlDef0->getShaderDefinition(shaderDef0);

    Aurora::MaterialShaderDefinition shaderDef1;
    pMtlDef1->getShaderDefinition(shaderDef1);
    ASSERT_TRUE(shaderDef0.compare(shaderDef1));

    Aurora::MaterialDefinitionPtr pMtlDef2 = matGen.generate(materialXString2);
    ASSERT_NE(pMtlDef2, nullptr);
    ASSERT_STREQ(pMtlDef2->source().uniqueId.c_str(), "MaterialX_58010dd45510ab94");
    ASSERT_EQ(pMtlDef2->defaults().properties.size(), 6);
    ASSERT_EQ(pMtlDef2->defaults().propertyDefinitions.size(), 6);
    ASSERT_EQ(pMtlDef2->defaults().textureNames.size(), 2);
    ASSERT_EQ(pMtlDef2->defaults().textures.size(), 2);
    ASSERT_STREQ(pMtlDef2->defaults().textureNames[1].c_str(), "opacity_image");
    ASSERT_STREQ(pMtlDef2->defaults().textures[1].defaultFilename.c_str(),
        "../Textures/fishscale_roughness.png");

    Aurora::MaterialShaderDefinition shaderDef2;
    pMtlDef2->getShaderDefinition(shaderDef2);
    ASSERT_FALSE(shaderDef0.compare(shaderDef2));

    Aurora::MaterialDefinitionPtr pMtlDef2Dupe = matGen.generate(materialXString2);
    Aurora::MaterialDefinition* pMtlDef2Raw    = pMtlDef2.get();
    ASSERT_EQ(pMtlDef2Dupe.get(), pMtlDef2Raw);
    ASSERT_EQ(pMtlDef2.use_count(), 2);
    pMtlDef2Dupe.reset();
    ASSERT_EQ(pMtlDef2.use_count(), 1);
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
    ASSERT_EQ(pShader0->libraryIndex(), 0);
    ASSERT_EQ(pShader0->entryPoints().size(), 2);

    Aurora::MaterialDefinitionPtr pMtlDef1 = matGen.generate(materialXString1);
    Aurora::MaterialShaderDefinition shaderDef1;
    pMtlDef1->getShaderDefinition(shaderDef1);

    Aurora::MaterialShaderPtr pShader1 = shaderLib.acquire(shaderDef1);
    ASSERT_NE(pShader1, nullptr);
    ASSERT_EQ(pShader1.get(), pShader0.get());

    Aurora::MaterialDefinitionPtr pMtlDef2 = matGen.generate(materialXString2);
    Aurora::MaterialShaderDefinition shaderDef2;
    pMtlDef2->getShaderDefinition(shaderDef2);

    Aurora::MaterialShaderPtr pShader2 = shaderLib.acquire(shaderDef2);
    ASSERT_NE(pShader2, nullptr);
    ASSERT_EQ(pShader2->libraryIndex(), 1);
    ASSERT_NE(pShader2.get(), pShader0.get());

    int numCompiled  = 0;
    int numDestroyed = 0;
    auto compileFunc = [&numCompiled](const Aurora::MaterialShader&) {
        numCompiled++;
        return true;
    };
    auto destroyFunc = [&numDestroyed](int) { numDestroyed++; };

    shaderLib.update(compileFunc, destroyFunc);
    ASSERT_EQ(numCompiled, 2);
    ASSERT_EQ(numDestroyed, 0);

    pShader2.reset();
    shaderLib.update(compileFunc, destroyFunc);
    ASSERT_EQ(numCompiled, 2);
    ASSERT_EQ(numDestroyed, 1);

    pShader0->incrementRefCount("foo");
    shaderLib.update(compileFunc, destroyFunc);
    ASSERT_EQ(numCompiled, 3);
    ASSERT_EQ(numDestroyed, 1);

    Aurora::MaterialShaderPtr pShader3 = shaderLib.acquire(shaderDef2);
    ASSERT_NE(pShader3, nullptr);
    ASSERT_EQ(pShader3->libraryIndex(), 1);

    shaderLib.update(compileFunc, destroyFunc);
    ASSERT_EQ(numCompiled, 4);
    ASSERT_EQ(numDestroyed, 1);
}

} // namespace

#endif
