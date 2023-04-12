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

#include <AuroraTestHelpers.h>
#include <BaselineImageHelpers.h>
#include <TestHelpers.h>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <streambuf>

using namespace Aurora;

namespace
{

class MaterialTest : public TestHelpers::FixtureBase
{
public:
    MaterialTest() {}

    void setupAssetPaths(const vector<string>& additionalPaths = {})
    {

        _paths = { "", dataPath() + "/Materials/", dataPath() + "/Textures/" };
        for (size_t i = 0; i < additionalPaths.size(); i++)
        {
            _paths.push_back(additionalPaths[i]);
        }
        // Setup the resource loading function to use asset search paths.
        auto loadResourceFunc = [this](const string& uri, vector<unsigned char>* pBufferOut,
                                    string* pFileNameOut) {
            // Iterate through all search paths.
            for (size_t i = 0; i < _paths.size(); i++)
            {
                // Prefix the current search path to URI.
                string currPath = _paths[i] + uri;
                ifstream is(currPath, ifstream::binary);

                // If file could not be opened, continue.
                if (!is)
                    continue;
                is.seekg(0, is.end);
                size_t length = is.tellg();
                is.seekg(0, is.beg);
                pBufferOut->resize(length);
                is.read((char*)&(*pBufferOut)[0], length);

                // Copy the URI to file name with no manipulation.
                *pFileNameOut = currPath;

                return true;
            }
            return false;
        };
        defaultRenderer()->setLoadResourceFunction(loadResourceFunc);
    }
    ~MaterialTest() {}

    // Load a MaterialX document and process file paths to correct locations for unit tests.
    string loadAndProcessMaterialXFile(const string& filename)
    {
        ifstream mtlXStream(filename);

        // Load source into string.
        string mtlXString((istreambuf_iterator<char>(mtlXStream)), istreambuf_iterator<char>());

        // Fail if file does not exist.
        if (mtlXString.empty())
            return "";

        // Replace the texture path
        string processedMtlXString =
            regex_replace(mtlXString, regex("\\.\\./Textures"), dataPath() + "\\Textures");

        return processedMtlXString;
    }

    vector<string> _paths;
};

// Test basic material functionality
TEST_P(MaterialTest, TestMaterialDefault)
{
    if (!backendSupported())
        return;

    // Create a pRenderer for this type
    IRenderer::Backend backend = rendererBackend();
    auto pRenderer             = createRenderer(backend);

    // Create a material
    IScenePtr pScene = pRenderer->createScene();
    Path testMaterial("testMaterial");
    pScene->setMaterialType(testMaterial);
    pScene->addPermanent(testMaterial);

    // Ensure the material properties can be set without error
    testFloatValue(*pScene, testMaterial, "base", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "base_color", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "specular_rotation", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "coat_IOR", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "transmission", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "diffuse_roughness", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "specular", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "specular_roughness", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "specular_anisotropy", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "specular_IOR", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "metalness", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "subsurface", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "subsurface_anisotropy", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "transmission_color", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "subsurface_scale", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "sheen", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "sheen_roughness", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatValue(*pScene, testMaterial, "coat", true,
        " material param test" + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "specular_color", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "subsurface_color", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "subsurface_radius", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "sheen_color", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloat3Value(*pScene, testMaterial, "sheen_color", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");
    testBooleanValue(*pScene, testMaterial, "thin_walled", true,
        " material param test " + to_string(__LINE__) + " (" + rendererDescription() + ")");

    pScene->removePermanent(testMaterial);
}

// Convenience function to clear all material properties
static void clearAllProperties(Properties& props)
{
    props["base"].clear();
    props["base_color"].clear();
    props["diffuse_roughness"].clear();
    props["specular"].clear();
    props["specular_color"].clear();
    props["specular_roughness"].clear();
    props["specular_IOR"].clear();
    props["specular_anisotropy"].clear();
    props["specular_rotation"].clear();
    props["metalness"].clear();
    props["transmission"].clear();
    props["transmission_color"].clear();
    props["subsurface"].clear();
    props["subsurface_color"].clear();
    props["subsurface_radius"].clear();
    props["subsurface_scale"].clear();
    props["subsurface_anisotropy"].clear();
    props["sheen"].clear();
    props["sheen_color"].clear();
    props["sheen_roughness"].clear();
    props["thin_walled"].clear();
    props["coat"].clear();
    props["coat_color"].clear();
    props["coat_roughness"].clear();
    props["coat_anisotropy"].clear();
    props["coat_rotation"].clear();
    props["coat_IOR"].clear();
    props["coat_affect_color"].clear();
    props["coat_affect_roughness"].clear();
    props["opacity"].clear();

    props["base_color_image"].clear();
    props["specular_roughness_image"].clear();
    props["specular_color_image"].clear();
    props["coat_color_image"].clear();
    props["coat_roughness_image"].clear();

    props["normal_image"].clear();
    props["displacement_image"].clear();
}

// Test material functionality using baseline image testing
TEST_P(MaterialTest, TestMaterialClearMaterialProperty)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create teapot geometry.
    Path geometry = createTeapotGeometry(*pScene);

    // Create a material (by setting default material type.)
    Path material("ClearMaterial");
    pScene->setMaterialType(material);

    Path inst("instance0");
    pScene->addInstance(inst, geometry, { { Names::InstanceProperties::kMaterial, material } });

    // Test color.
    vec3 bc0(1, 0, 0.25f);

    // Set the color property.
    Properties matProps;
    matProps["base_color"] = bc0;

    // Set the test color
    pScene->setMaterialProperties(material, matProps);

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_BeforeClear", "Materials");

    // Clear the color property.
    matProps["base_color"].clear();

    // Clear the test color
    pScene->setMaterialProperties(material, matProps);

    // Render the scene and check baseline image.
    // On rasteriser this will appear incorrect (the previous color will still be used, see
    // OGSMOD-858)
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_AfterClear", "Materials");
}

// Convenience function to create a grid of teapots, with a single default material.
Paths createTeapotGrid(TestHelpers::FixtureBase& pFixture, uint32_t gridWidth, uint32_t gridHeight)
{
    auto pScene    = pFixture.defaultScene();
    auto pRenderer = pFixture.defaultRenderer();

    // Create shared geom for all instances.
    Path geometry = pFixture.createTeapotGeometry(*pScene);

    // Create grid parameters.
    const vec3 gridDims(gridWidth - 1, gridHeight - 1, 0.0f);
    const vec3 gridOrigin(0, 0, +17.0f);
    const vec3 gridGap(-3.1f, 3.1f, 0.0f); // Invert X so 0->1 is left to right
    const vec3 gridHalfSize = gridGap * gridDims * 0.5f;

    // All teapots created with the same default material.
    Path material("TeapotMaterial");
    pScene->setMaterialType(material);

    InstanceDefinitions instDefs;

    // Create a grid of teapots by adding add all the instances to the scene.
    // Material and transform variation is specified though a callback when the instances are
    // constructed.
    for (uint32_t index = 0; index < (gridWidth * gridHeight); index++)
    {
        size_t i     = index / gridWidth;
        size_t j     = index % gridHeight;
        vec3 gridPos = vec3(j, i, 0) * gridGap + gridOrigin - gridHalfSize;
        instDefs.push_back({ "instance" + to_string(index),
            { { Names::InstanceProperties::kTransform, translate(gridPos) },
                { Names::InstanceProperties::kMaterial, material } } });
    }

    return pScene->addInstances(geometry, instDefs);
}

TEST_P(MaterialTest, DISABLED_TestMaterialDuplicateMaterialTypes)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a grid of teapot instances.
    vector<Path> teapotGrid = createTeapotGrid(*this, 6, 12);

    // Test with all the built-ins except for the first one. This is the "Default" which refers to
    // one of the other material types, so it would be redundant to test with it.
    const vector<string> builtIns = pRenderer->builtInMaterials();
    int materialCount             = 72;

    // Create materials for each teapot.
    for (int i = 0; i < teapotGrid.size(); i++)
    {
        // Read the materialX document from file.
        int mtlIdx          = i % materialCount;
        string mtlXName     = "standardSurface_" + to_string(mtlIdx) + "Dumped";
        string mtlXFullPath = dataPath() + "/Materials/dumpedMtlx/" + mtlXName + ".mtlx";

        // Load and process the MaterialX document and ensure it loaded correctly.
        string processedMtlXString = loadAndProcessMaterialXFile(mtlXFullPath);
        EXPECT_FALSE(processedMtlXString.empty());

        // Create material from document
        Path dupeMat = "DupeMaterial" + to_string(i);
        pScene->setMaterialType(dupeMat, Names::MaterialTypes::kMaterialX, processedMtlXString);

        pScene->setInstanceProperties(
            teapotGrid[i], { { Names::InstanceProperties::kMaterial, dupeMat } });
    }

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Materials");
}

// Test basic material properties using baseline image testing
TEST_P(MaterialTest, TestMaterialBasicMaterialProperties)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a grid of teapot instances.
    const uint32_t gridWidth  = 5;
    const uint32_t gridHeight = 5;
    vector<Path> teapotGrid   = createTeapotGrid(*this, 5, 5);
    vector<Path> materialGrid(teapotGrid.size());

    // Test with reference BSDF and Standard Surface BSDF.
    for (int useReference = 0; useReference < 2; useReference++)
    {
        // Enabled reference BSDF via options.
        pRenderer->options().setBoolean("isReferenceBSDFEnabled", useReference);
        string mtlType = useReference ? "Reference" : "StandardSurface";

        // Create materials for each teapot.
        for (size_t i = 0; i < materialGrid.size(); i++)
        {
            Properties matProps;
            materialGrid[i] = "BasicMaterial" + mtlType + to_string(i);
            pScene->setMaterialType(materialGrid[i]);

            Properties instProps;
            instProps[Names::InstanceProperties::kMaterial] = materialGrid[i];
            pScene->setInstanceProperties(teapotGrid[i], instProps);
        }

        // Test colors.
        vec3 bc0(1, 0.4f, 0.25f);
        vec3 bc1(0.35f, 1, 0.3f);

        // Test base_color (X-axis) against base (Y-axis.)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Define the base_color color
                vec3 color             = mix(bc0, bc1, u);
                matProps["base_color"] = color;

                // Define the base scalar
                matProps["base"] = v * 1.2f;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_Base_" + mtlType, "Materials");

        // Test specular_color (X-axis) against specular (Y-axis)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];

                // Reset the old values
                Properties matProps;
                clearAllProperties(matProps);

                // Define the specular_color color
                vec3 color                 = mix(bc0, bc1, u);
                matProps["specular_color"] = color;

                // Define the specular scalar
                matProps["specular"] = v * 2.0f;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Specular_" + mtlType, "Materials");

        // Test diffuse_roughness (X-axis) against specular_roughness (Y-axis)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];

                // Reset the old values
                Properties matProps;
                clearAllProperties(matProps);

                // Zero metalness (setting for clarity, is actually the default)
                matProps["metalness"] = 0.0f;

                // Constant specular/base properties
                matProps["specular"]       = 0.5f;
                matProps["base"]           = 0.5f;
                matProps["specular_color"] = bc0;
                matProps["base_color"]     = bc1;

                // Vary diffuse and specular roughness with U and V values.
                matProps["diffuse_roughness"]  = u;
                matProps["specular_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Roughness_" + mtlType, "Materials");

        // Test diffuse_roughness (X-axis) against specular_roughness (Y-axis), with tiny metalness
        // value. This should be no different to zero but is very different (see OGSMOD-867)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate from Y grid position.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.from X grid position.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];

                // Reset the old values
                Properties matProps;
                clearAllProperties(matProps);

                // Tiny metalness (should be close enough to zero to be indistinguishable)
                matProps["metalness"] = 0.001f;

                // Constant specular/base properties.
                matProps["specular"]       = 0.5f;
                matProps["base"]           = 0.5f;
                matProps["specular_color"] = bc0;
                matProps["base_color"]     = bc1;

                // Vary diffuse and specular roughness with U and V values.
                matProps["diffuse_roughness"]  = u;
                matProps["specular_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Roughness_TinyMetal_" + mtlType, "Materials");

        // Test metalness (X-axis) against specular_roughness (Y-axis)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate from Y grid position.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.from X grid position.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];

                // Reset the old values.
                Properties matProps;
                clearAllProperties(matProps);

                // Constant specular and base properties.
                matProps["specular"]          = 0.75f;
                matProps["base"]              = 0.25f;
                matProps["specular_color"]    = bc0;
                matProps["base_color"]        = bc1;
                matProps["diffuse_roughness"] = 0.5f;

                // Vary metalness and specular roughness with U and V values.
                matProps["metalness"]          = u;
                matProps["specular_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Metalness_" + mtlType, "Materials");
    }
}

// Set just the transmission value
TEST_P(MaterialTest, TestMaterialSetTransmission)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create teapot instance.
    Path geometry = createTeapotGeometry(*pScene);

    // Create the material.
    Path material("TransmissionMaterial");
    pScene->setMaterialProperties(material, { { "transmission", 0.5f } });

    // Create an instance with the material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = material;
    EXPECT_TRUE(pScene->addInstance(Path("instance0"), geometry, instProps));

    // Should render without crashing.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());

    // Wait for frame to avoid crash on exit
    pRenderer->waitForTask();
}

// Test more advanced material properties using baseline image testing
TEST_P(MaterialTest, TestMaterialAdvancedMaterialProperties)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a grid of teapot instances.
    const uint32_t gridWidth  = 5;
    const uint32_t gridHeight = 5;
    Paths teapotGrid          = createTeapotGrid(*this, 5, 5);
    vector<Path> materialGrid(teapotGrid.size());

    // Test with reference BSDF and Standard Surface BSDF.
    for (int useReference = 0; useReference < 2; useReference++)
    {
        // Enabled reference BSDF via options.
        pRenderer->options().setBoolean("isReferenceBSDFEnabled", useReference);
        string mtlType = useReference ? "Reference" : "StandardSurface";

        // Create materials for each teapot.
        for (size_t i = 0; i < materialGrid.size(); i++)
        {
            materialGrid[i] = "AdvancedMaterial" + mtlType + to_string(i);
            pScene->setMaterialType(materialGrid[i]);

            pScene->setInstanceProperties(
                teapotGrid[i], { { Names::InstanceProperties::kMaterial, materialGrid[i] } });
        }

        // Test colors.
        vec3 bc0(1, 0.4f, 0.25f);
        vec3 bc1(0.35f, 1, 0.3f);
        vec3 bc2(0.3f, 0.2f, 0.95f);
        vec3 bc3(0.7f, 0.8f, 0.05f);
        vec3 grey(0.25f, 0.25f, 0.25f);

        // Test subsurface color (X-axis) against subsurface radius (Y-axis)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.from Y grid position.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.from X grid position.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Reset the old values.
                clearAllProperties(matProps);

                // Constant base color.and sheen.
                matProps["base_color"] = grey;
                matProps["subsurface"] = 0.75f;

                // Vary subsurface color and subsurface radius with U and V values.
                vec3 color                    = mix(bc0, bc1, u);
                matProps["subsurface_color"]  = color;
                vec3 radius                   = mix(bc2, bc3, v);
                matProps["subsurface_radius"] = radius;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Subsurface_" + mtlType, "Materials");

        // Test sheen color (X-axis) against sheen roughness (Y-axis)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate from Y grid position.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.from X grid position.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Reset the old values.
                clearAllProperties(matProps);

                // Constant base color.and sheen.
                matProps["base_color"] = grey;
                matProps["sheen"]      = 1.0f;

                // Vary sheen color and sheen roughness with U and V values.
                vec3 color                  = mix(bc0, bc1, u);
                matProps["sheen_color"]     = color;
                matProps["sheen_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Sheen_" + mtlType, "Materials");

        // Test coat_color (X-axis) against coat_roughness (Y-axis.)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Reset the old values.
                clearAllProperties(matProps);

                // Constant coat, specular and base properties.
                matProps["coat"]     = 1.0f;
                matProps["coat_IOR"] = 0.5f;
                matProps["base"]     = 0.0f;
                matProps["specular"] = 0.0f;

                // Vary the coat_color color with U
                vec3 color             = mix(bc0, bc1, u);
                matProps["coat_color"] = color;

                // Vary the coat_roughness color with V
                matProps["coat_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        // TODO: The PT does not support this currently (see OGSMOD-699)
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_Coat_" + mtlType, "Materials");

        // Test coat_affect_color (X-axis) against coat_affect_roughness (Y-axis.)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Reset the old values.
                clearAllProperties(matProps);

                // Constant coat, specular and base properties.
                matProps["coat"]       = 1.0f;
                matProps["coat_IOR"]   = 0.5f;
                matProps["base"]       = 0.0f;
                matProps["specular"]   = 0.0f;
                matProps["coat_color"] = bc0;

                // Vary the coat_affect_color color with U
                matProps["coat_affect_color"] = u;

                // Vary the coat_affect_roughness color with V
                matProps["coat_affect_roughness"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        // TODO: The PT does not support this currently (see OGSMOD-699)
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_CoatAffect_" + mtlType, "Materials");

        // Test base_color (X-axis) against base (Y-axis.)
        for (uint32_t i = 0; i < gridHeight; i++)
        {
            // Calculate the V coordinate.
            float v = i / static_cast<float>(gridHeight - 1.0f);
            for (uint32_t j = 0; j < gridWidth; j++)
            {
                // Calculate the U coordinate.
                float u = j / static_cast<float>(gridWidth - 1.0f);

                // Get the material from the grid.
                Path material = materialGrid[i * gridWidth + j];
                Properties matProps;

                // Reset the old values.
                clearAllProperties(matProps);

                // Constant specular and base properties.
                matProps["base"]     = 0.75f;
                matProps["specular"] = 0.25f;

                // Vary the specular parameter with U.
                matProps["specular"] = u;

                // Vary the transmission parameter with V
                matProps["transmission"] = v;

                // Update the material.
                pScene->setMaterialProperties(material, matProps);
            }
        }

        // Render the scene and check baseline image.
        // TODO: PT does not support transmission currently (see OGSMOD-699)
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + "_Transmission_" + mtlType, "Materials");
    }
}

// Test material type creation using MaterialX
TEST_P(MaterialTest, TestMaterialTypes)
{
    // No MaterialX on HGI yet.
    if (!isDirectX())
        return;

    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    setDefaultRendererPathTracingIterations(256);

    // Set an invalid material type.
    pScene->setMaterialType("BadMaterial", "NotAMaterialType", "");

    // Add permanent reference to force creation of material, which will trigger exception.
    ASSERT_THROW(pScene->addPermanent("BadMaterial"), TestHelpers::AuroraLoggerException);

    // We currently have one built-in.
    const vector<string> builtIns = pRenderer->builtInMaterials();
    ASSERT_EQ(builtIns.size(), 1);
    ASSERT_EQ(builtIns[0].compare("Default"), 0);

    // Set bad material built-in (on PT this will return default material type and print error.)
    pScene->setMaterialType(Path("nullMtl"), Names::MaterialTypes::kBuiltIn, "NotABuiltIn");

    // Add permanent reference to force creation of material (this will trigger error message).
    pScene->addPermanent("nullMtl");
    ASSERT_NE(lastLogMessage().find("Unknown built-in material type NotABuiltIn for material"),
        string::npos);

    // clang-format off
    // Test Material
    string testMtl =
        "<?xml version=\"1.0\" ?>\n"
        "<materialx version=\"1.37\">\n"
        "  <material name=\"Test_Material\">\n"
        "    <shaderref name=\"SR_Test1\" node=\"standard_surface\">\n"
        "      <bindinput name=\"base\" type=\"float\" value=\"1.0\" />\n"
        "      <bindinput name=\"base_color\" type=\"color3\" value=\"0.0, 1.0, 0.0\" />\n"
        "      <bindinput name=\"specular_color\" type=\"color3\" value=\"1.0, 1.0, 1.0\" />\n"
        "      <bindinput name=\"specular_roughness\" type=\"float\" value=\"0.3\" />\n"
        "    </shaderref>\n"
        "  </material>\n"
        "</materialx>";
    // clang-format on

    Path geometry = createTeapotGeometry(*pScene);

    // Create materialX material from document.
    Path mtl0("Mtl0");
    pScene->setMaterialType(mtl0, Names::MaterialTypes::kMaterialX, testMtl);

    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial]  = mtl0;
    instProps[Names::InstanceProperties::kTransform] = translate(vec3(-3, 0, 6));
    EXPECT_TRUE(pScene->addInstance(Path("I0"), geometry, instProps));

    // Create materialX material from path.
    Path mtl1("Mtl1");
    pScene->setMaterialType(
        mtl1, Names::MaterialTypes::kMaterialXPath, dataPath() + "/Materials/TestMaterial.mtlx");

    instProps[Names::InstanceProperties::kMaterial]  = mtl1;
    instProps[Names::InstanceProperties::kTransform] = translate(vec3(0, 0, 6));
    EXPECT_TRUE(pScene->addInstance(Path("I1"), geometry, instProps));

    // Create materialX material from path.
    Path mtl2("Mtl2");
    pScene->setMaterialType(mtl2, Names::MaterialTypes::kBuiltIn, "Default");

    instProps[Names::InstanceProperties::kMaterial]  = mtl1;
    instProps[Names::InstanceProperties::kTransform] = translate(vec3(3, 0, 6));
    EXPECT_TRUE(pScene->addInstance(Path("I2"), geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Materials");
}

// Test material creation using MaterialX
TEST_P(MaterialTest, TestMaterialX)
{
    // No MaterialX on HGI yet.
    if (!isDirectX())
        return;

    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a material with invalid document.
    pScene->setMaterialType("NullMaterial", Names::MaterialTypes::kMaterialX,
        "Not valid materialX! $#$(#*(%*#,.,.,.<><><>");

    // Error occurs when material resource is activated.
    pScene->addPermanent("NullMaterial");
    ASSERT_NE(lastLogMessage().find("Failed to generate MaterialX code."), string::npos)
        << lastLogMessage();

    // clang-format off
    // Test Material
    string testMtl =
        "<?xml version=\"1.0\" ?>\n"
        "<materialx version=\"1.37\">\n"
        "  <nodegraph name=\"NG_Test1\">\n"
        "    <position name=\"positionWorld\" type=\"vector3\" />\n"
        "    <constant name=\"constant_1\" type=\"color3\">\n"
        "      <parameter name=\"value\" type=\"color3\" value=\"1.0, 0.1, 0.0\" />\n"
        "    </constant>\n"
        "    <constant name=\"constant_2\" type=\"color3\">\n"
        "      <parameter name=\"value\" type=\"color3\" value=\"0.0, 0.0, 0.0\" />\n"
        "    </constant>\n"
        "    <constant name=\"mix_amount\" type=\"float\">\n"
        "      <parameter name=\"value\" type=\"float\" value=\"0.6\" />\n"
        "    </constant>\n"
        "    <mix name=\"mix1\" type=\"color3\">\n"
        "      <input name=\"mix\" type=\"float\" nodename=\"mix_amount\" />\n"
        "      <input name=\"bg\" type=\"color3\" nodename=\"constant_1\" />\n"
        "      <input name=\"fg\" type=\"color3\" nodename=\"constant_2\" />\n"
        "    </mix>\n"
        "    <output name=\"out\" type=\"color3\" nodename=\"mix1\" />\n"
        "  </nodegraph>\n"
        "\n"
        "  <material name=\"Test_Material\">\n"
        "    <shaderref name=\"SR_Test1\" node=\"standard_surface\">\n"
        "      <bindinput name=\"base\" type=\"float\" value=\"1.0\" />\n"
        "      <bindinput name=\"base_color\" type=\"color3\" nodegraph=\"NG_Test1\" output=\"out\" />\n"
        "      <bindinput name=\"specular_color\" type=\"color3\" value=\"1.0, 1.0, 1.0\" />\n"
        "      <bindinput name=\"specular_roughness\" type=\"float\" value=\"0.6\" />\n"
        "    </shaderref>\n"
        "  </material>\n"
        "</materialx>";
    // clang-format on

    // Create teapot instance.
    Path geometry = createTeapotGeometry(*pScene);
    Path testMaterial("TestMaterialX");
    pScene->setMaterialType(testMaterial, Names::MaterialTypes::kMaterialX, testMtl);

    Path instance("MaterialXInstance");
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = testMaterial;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_FromString", "Materials");

    pScene->setMaterialProperties(testMaterial, { { "mix_amount/value", 0.9f } });

    pScene->setInstanceProperties(instance, instProps);

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_ParameterOverride", "Materials");
}

// Test material creation using MaterialX file dumped from HdAurora.
TEST_P(MaterialTest, TestHdAuroraMaterialX)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    if (!pRenderer)
        return;

    setDefaultRendererPathTracingIterations(256);

    setupAssetPaths();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create teapot geom.
    Path geometry = createTeapotGeometry(*pScene);

    // Try loading a MtlX file dumped from HdAurora.
    Path material("HdAuroraMaterial");
    pScene->setMaterialType(material, Names::MaterialTypes::kMaterialXPath,
        dataPath() + "/Materials/HdAuroraTest.mtlx");

    // Add to scene.
    Path instance("HdAuroraInstance");
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = material;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_HdAuroraMtlX", "Materials");
}

// Test material creation using MaterialX file dumped from HdAurora that has a texture.
TEST_P(MaterialTest, TestHdAuroraTextureMaterialX)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    if (!pRenderer)
        return;

    setDefaultRendererPathTracingIterations(256);

    setupAssetPaths();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create teapot geom.
    Path geometry = createTeapotGeometry(*pScene);

    // Try loading a MtlX file dumped from HdAurora.
    Path material("HdAuroraMaterial");
    pScene->setMaterialType(material, Names::MaterialTypes::kMaterialXPath,
        dataPath() + "/Materials/HdAuroraTextureTest.mtlx");

    // Add to scene.
    Path instance("HdAuroraInstance");
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = material;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_HdAuroraMtlX", "Materials");
}

// Test different settings for isFlipImageYEnabled option.
// Disabled as this testcase fails with error in MaterialGenerator::generate
TEST_P(MaterialTest, DISABLED_TestMaterialXFlipImageY)
{
    // No MaterialX on HGI yet.
    if (!isDirectX())
        return;

    for (int flipped = 0; flipped < 2; flipped++)
    {
        // Create the default scene (also creates renderer)
        auto pScene    = createDefaultScene();
        auto pRenderer = defaultRenderer();
        setDefaultRendererPathTracingIterations(256);

        // If pRenderer is null this renderer type not supported, skip rest of the test.
        if (!pRenderer)
            return;

        // Set the flip-Y option.
        Properties options = { { "isFlipImageYEnabled", bool(flipped != 0) } };
        pRenderer->setOptions(options);

        // Create teapot geom.
        Path geometry = createTeapotGeometry(*pScene);

        // Read the materialX document from file.
        string materialXFullPath = dataPath() + "/Materials/TestTexture.mtlx";

        // Load source into string.
        string processedMaterialXString = loadAndProcessMaterialXFile(materialXFullPath);
        // Fail if file does not exist.
        ASSERT_FALSE(processedMaterialXString.empty());

        // Create material with current flip-Y value.
        Path material("Texture");
        pScene->setMaterialType(
            material, Names::MaterialTypes::kMaterialX, processedMaterialXString);

        // Add to scene.
        Path instance(flipped ? "_Flipped" : "_NotFlipped");
        Properties instProps;
        instProps[Names::InstanceProperties::kMaterial] = material;
        EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

        // Render the scene and check baseline image.
        ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(
            currentTestName() + (flipped ? "_Flipped" : "_NotFlipped"), "Materials");
    }
}

// Test different MtlX file that loads a BMP.
// Disabled as this testcase fails with error in MaterialGenerator::generate
TEST_P(MaterialTest, DISABLED_TestMaterialXBMP)
{

    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // No MaterialX on HGI yet.
    if (!isDirectX())
        return;

    // Create teapot geom.
    Path geometry = createTeapotGeometry(*pScene);

    // Read the materialX document from file.
    string materialXFullPath = dataPath() + "/Materials/TestBMPTexture.mtlx";

    // Load and process the MaterialX document and ensure it loaded correctly.
    string processedMtlXString = loadAndProcessMaterialXFile(materialXFullPath);
    EXPECT_FALSE(processedMtlXString.empty());

    // Create material with current flip-Y value.
    Path material("Texture");
    pScene->setMaterialType(material, Names::MaterialTypes::kMaterialX, processedMtlXString);

    // Add to scene.
    Path instance("Mtl");
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = material;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Materials");
}

TEST_P(MaterialTest, TestMaterialTransparency)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Constant colors.
    vec3 color0(0.5f, 1.0f, 0.3f);
    vec3 opacity0(0.1f, 0.25f, 0.1f);

    // Create geometry for teapot and plane geometry.
    Path planeGeom  = createPlaneGeometry(*pScene);
    Path teapotGeom = createTeapotGeometry(*pScene);

    // Create materials.
    Path transpMtl("TransparentMaterial");
    pScene->setMaterialProperties(transpMtl, {});
    Path opaqueMtl("OpaqueMaterial");
    pScene->setMaterialProperties(opaqueMtl, { { "base_color", color0 } });

    // Create opaque plane instance behind a transparent teapot instance.
    mat4 xform = translate(vec3(0, 0.5f, +2));
    Path planeInst("PlaneInstance");
    Properties planeInstProps;
    planeInstProps[Names::InstanceProperties::kMaterial]  = opaqueMtl;
    planeInstProps[Names::InstanceProperties::kTransform] = xform;
    EXPECT_TRUE(pScene->addInstance(planeInst, planeGeom, planeInstProps));

    Path teapotInst("TeapotInstance");
    Properties teapotInstProps;
    teapotInstProps[Names::InstanceProperties::kMaterial] = transpMtl;
    EXPECT_TRUE(pScene->addInstance(teapotInst, teapotGeom, teapotInstProps));

    // Render baseline image with transmission.
    pScene->setMaterialProperties(
        transpMtl, { { "transmission", 0.75f }, { "thin_walled", false } });
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "Transmission", "Materials");

    // Render baseline image with transmission and thin_walled flag set.
    pScene->setMaterialProperties(
        transpMtl, { { "transmission", 0.75f }, { "thin_walled", true } });
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "ThinWalled", "Materials");

    // Render baseline image with opacity, no transmission and thin_walled flag not set.
    pScene->setMaterialProperties(
        transpMtl, { { "transmission", 0.0f }, { "opacity", opacity0 }, { "thin_walled", false } });
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "Opacity", "Materials");
}

// Disabled as this testcase fails with error in MaterialGenerator::generate
TEST_P(MaterialTest, DISABLED_TestMtlXSamplers)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    setupAssetPaths();

    Properties options = { { "isFlipImageYEnabled", true } };
    pRenderer->setOptions(options);

    string materialLayer0XFullPath        = dataPath() + "/Materials/Decals/test_decal_mask.mtlx";
    string processedLayer0MaterialXString = loadAndProcessMaterialXFile(materialLayer0XFullPath);
    ASSERT_FALSE(processedLayer0MaterialXString.empty());

    Path kClampMaterialXPath("ClampMtlX");

    pScene->setMaterialType(
        kClampMaterialXPath, Names::MaterialTypes::kMaterialX, processedLayer0MaterialXString);

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene, vec2(2, 2), vec2(-0.5, -0.5));

    // Create instance with material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = kClampMaterialXPath;
    Path kInstance                                  = "SamplerInstance";
    EXPECT_TRUE(pScene->addInstance(kInstance, geomPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Materials");
}

// MaterialX as layered materials
// Disabled as this testcase fails with error in MaterialGenerator::generate
TEST_P(MaterialTest, DISABLED_TestMaterialMaterialXLayers)
{
    // No MaterialX on HGI yet.
    if (!isDirectX())
        return;

    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    setupAssetPaths();

    Properties options = { { "isFlipImageYEnabled", false } };
    pRenderer->setOptions(options);

    std::string proteinXFullPath        = dataPath() + "/Materials/FishScale.mtlx";
    string processedBaseMaterialXString = loadAndProcessMaterialXFile(proteinXFullPath);
    ASSERT_FALSE(processedBaseMaterialXString.empty());

    string materialLayer0XFullPath        = dataPath() + "/Materials/Decals/test_decal_mask.mtlx";
    string processedLayer0MaterialXString = loadAndProcessMaterialXFile(materialLayer0XFullPath);
    ASSERT_FALSE(processedLayer0MaterialXString.empty());

    string materialLayer1XFullPath = dataPath() + "/Materials/Decals/test_decal_transparent.mtlx";
    string processedLayer1MaterialXString = loadAndProcessMaterialXFile(materialLayer1XFullPath);
    ASSERT_FALSE(processedLayer1MaterialXString.empty());

    vector<Path> materialLayers = { "LayerMaterial0", "LayerMaterial1", "LayerMaterial1",
        "LayerMaterial0" };
    pScene->setMaterialType(
        materialLayers[0], Names::MaterialTypes::kMaterialX, processedLayer0MaterialXString);
    pScene->setMaterialType(
        materialLayers[1], Names::MaterialTypes::kMaterialX, processedLayer1MaterialXString);

    Path baseMaterial("BaseMaterial");
    pScene->setMaterialType(
        baseMaterial, Names::MaterialTypes::kMaterialX, processedBaseMaterialXString);

    int numLayers = 4;
    vector<string> geometryLayers;

    float scales[]      = { 1, 2, 1.5f, 0.4f };
    glm::vec2 origins[] = { glm::vec2(0.75f, 1.0f), glm::vec2(-0.25f, 1.2f), glm::vec2(0.25f, 0.6f),
        glm::vec2(-0.5f, 1.4f) };

    vector<vector<glm::vec2>> xformedUVs(numLayers);

    for (int layer = 0; layer < numLayers; layer++)
    {
        auto& uvs = xformedUVs[layer];
        uvs.resize(TestHelpers::TeapotModel::verticesCount());

        for (uint32_t i = 0; i < uvs.size(); i++)
        {
            uvs[i].x = *(TestHelpers::TeapotModel::vertices() + (i * 3 + 0));
            uvs[i].y = *(TestHelpers::TeapotModel::vertices() + (i * 3 + 1));
            uvs[i]   = origins[layer] - uvs[i];
            uvs[i] *= scales[layer];
        }

        const Path kDecalUVGeomPath = "DecalUVGeomPath" + to_string(layer);
        GeometryDescriptor geomDesc;
        geomDesc.type = PrimitiveType::Triangles;
        geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] =
            AttributeFormat::Float2;
        geomDesc.vertexDesc.count = TestHelpers::TeapotModel::verticesCount();
        geomDesc.getAttributeData = [&xformedUVs, layer](AttributeDataMap& buffers,
                                        size_t firstVertex, size_t vertexCount, size_t firstIndex,
                                        size_t indexCount) {
            AU_ASSERT(firstVertex == 0, "Partial update not supported");
            AU_ASSERT(vertexCount == TestHelpers::TeapotModel::verticesCount(),
                "Partial update not supported");
            AU_ASSERT(firstIndex == 0, "Partial update not supported");
            AU_ASSERT(indexCount == 0, "Partial update not supported");
            buffers[Names::VertexAttributes::kTexCoord0].address = xformedUVs[layer].data();
            buffers[Names::VertexAttributes::kTexCoord0].size =
                TestHelpers::TeapotModel::verticesCount() * sizeof(vec2);
            buffers[Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

            return true;
        };
        pScene->setGeometryDescriptor(kDecalUVGeomPath, geomDesc);
    }

    for (int layer = 0; layer < numLayers; layer++)
    {
        const Path kDecalUVGeomPath = "DecalUVGeomPath" + to_string(layer);
        geometryLayers.push_back(kDecalUVGeomPath);
    }

    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    Path planeGeometry = createPlaneGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance("TeapotInstance", geometry,
        { { Names::InstanceProperties::kMaterial, baseMaterial },
            { Names::InstanceProperties::kMaterialLayers, materialLayers },
            { Names::InstanceProperties::kGeometryLayers, geometryLayers },
            { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) } }));
    EXPECT_TRUE(pScene->addInstance(
        "Plane", planeGeometry, { { Names::InstanceProperties::kMaterial, baseMaterial } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());

    // Remove teapot and replace with one with more layers.
    pScene->removeInstance("TeapotInstance");
    EXPECT_TRUE(pScene->addInstance("AnotherTeapotInstance", geometry,
        { { Names::InstanceProperties::kMaterial, baseMaterial },
            { Names::InstanceProperties::kMaterialLayers,
                vector<Path>({ materialLayers[3], materialLayers[2], materialLayers[1],
                    materialLayers[0], materialLayers[2], materialLayers[3] }) },
            { Names::InstanceProperties::kGeometryLayers,
                vector<Path>({ geometryLayers[0], geometryLayers[0], geometryLayers[1],
                    geometryLayers[0], geometryLayers[2], geometryLayers[3] }) },
            { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName() + "_Removed");
}

// Test object space normal in materialX.
TEST_P(MaterialTest, DISABLED_TestObjectSpaceMaterialX)
{

    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    setDefaultRendererPathTracingIterations(256);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create teapot geom.
    Path geometry = createTeapotGeometry(*pScene);

    // Try loading a MtlX file dumped from Oxide.
    Path material("ThreadMaterial");
    pScene->setMaterialType(
        material, Names::MaterialTypes::kMaterialXPath, dataPath() + "/Materials/TestThread.mtlx");

    // Add to scene.
    Path instance("ThreadInstance");
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = material;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "_ThreadMtlX", "Materials");
}


INSTANTIATE_TEST_SUITE_P(MaterialTests, MaterialTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
