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

// Disable unit test as causes failure in debug mode
#if _DEBUG
#define DISABLE_UNIT_TESTS
#endif

#if !defined(DISABLE_UNIT_TESTS)

#include <chrono>
#include <cmath>

#include "AuroraTestHelpers.h"
#include "BaselineImageHelpers.h"

#include <fstream>

using namespace Aurora;

namespace
{

class ImageTest : public TestHelpers::FixtureBase
{
public:
    ImageTest() {}
    ~ImageTest() {}
};

// Basic image test.
TEST_P(ImageTest, TestImageDefault)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Load pixels for test image file.
    const std::string txtName = dataPath() + "/Textures/Hieroglyphs.jpg";

    // Create a material.
    const Path kMaterialPath = "DefaultMaterial";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color_image", loadImage(txtName) } });

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene);

    // Create instance with material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = kMaterialPath;
    EXPECT_TRUE(pScene->addInstance(nextPath(), geomPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Images");
}

// Basic image test.
// TODO: Re-enable once samplers working.
TEST_P(ImageTest, TestImageSamplers)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Load pixels for test image file.
    const std::string txtName = dataPath() + "/Textures/Mandrill.png";
    Path imagePath            = loadImage(txtName);

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene, vec2(2, 2), vec2(-0.5, -0.5));

    const Path kClampSamplerPath = "ClampSampler";
    pScene->setSamplerProperties(kClampSamplerPath,
        {
            { Aurora::Names::SamplerProperties::kAddressModeU,
                Aurora::Names::AddressModes::kClamp },
            { Aurora::Names::SamplerProperties::kAddressModeV,
                Aurora::Names::AddressModes::kClamp },
        });

    // Create a material.
    const Path kClampMaterialPath = "ClampSamplerMaterial";
    pScene->setMaterialProperties(kClampMaterialPath,
        { { "base_color_image", imagePath },
            {
                "base_color_image_sampler",
                kClampSamplerPath,
            } });

    // Create instance with material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = kClampMaterialPath;
    Path kInstance                                  = "SamplerInstance";
    EXPECT_TRUE(pScene->addInstance(kInstance, geomPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "Clamp", "Images");

    const Path kMirrorSamplerPath = "MirrorSampler";
    pScene->setSamplerProperties(kMirrorSamplerPath,
        {
            { Aurora::Names::SamplerProperties::kAddressModeU,
                Aurora::Names::AddressModes::kMirror },
            { Aurora::Names::SamplerProperties::kAddressModeV,
                Aurora::Names::AddressModes::kMirror },
        });

    const Path kMirrorMaterialPath = "MirrorSamplerMaterial";
    pScene->setMaterialProperties(kMirrorMaterialPath,
        { { "base_color_image", imagePath },
            {
                "base_color_image_sampler",
                kMirrorSamplerPath,
            } });

    pScene->setInstanceProperties(
        kInstance, { { Aurora::Names::InstanceProperties::kMaterial, kMirrorMaterialPath } });

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "Mirror", "Images");
}

// Basic image test.
TEST_P(ImageTest, TestImageOpacity)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Path to test image.
    const std::string txtName = dataPath() + "/Textures/Triangle.png";

    // Create a material.
    const Path kMaterialPath = "OpacityMaterial";
    pScene->setMaterialProperties(kMaterialPath, { { "opacity_image", loadImage(txtName) } });

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene);

    // Create instance with material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial] = kMaterialPath;
    EXPECT_TRUE(pScene->addInstance(nextPath(), geomPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Images");
}

// Add test for sRGB->linear conversion.
TEST_P(ImageTest, TestGammaImage)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Load pixels for test image file.
    const std::string txtName = dataPath() + "/Textures/Mandrill.png";
    Path linearImagePath      = loadImage(txtName, true);

    // Create the same image.with linearize set to false.
    Path gammaImagePath = loadImage(txtName, false);

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene);

    // Create matrix and material for first instance (which is linearized).
    const Path kMaterialPath0 = "GammaMaterial0";
    pScene->setMaterialProperties(kMaterialPath0, { { "base_color_image", linearImagePath } });

    mat4 mtx0 = translate(glm::vec3(-1.0, 0, -0.4));

    // Create matrix and material for second instance (which is not linearized).
    const Path kMaterialPath1 = "GammaMaterial1";
    pScene->setMaterialProperties(kMaterialPath1, { { "base_color_image", gammaImagePath } });
    mat4 mtx1 = translate(glm::vec3(+1.0, 0, -0.4));

    // Set the test image

    // Create instances with materials.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath0;
    instProps[Names::InstanceProperties::kTransform] = mtx0;
    EXPECT_TRUE(pScene->addInstance(nextPath(), geomPath, instProps));
    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath1;
    instProps[Names::InstanceProperties::kTransform] = mtx1;
    EXPECT_TRUE(pScene->addInstance(nextPath(), geomPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Images");
}

// Normal map image test.
TEST_P(ImageTest, TestNormalMapImage)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kDirection, value_ptr(glm::vec3(0.0f, -0.25f, +1.0f)));
    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kColor, value_ptr(glm::vec3(1, 1, 1)));

    // Path to test normal map
    const std::string txtName = dataPath() + "/Textures/fishscale_normal.png";

    // Create geometry.
    Path planePath  = createPlaneGeometry(*pScene);
    Path teapotPath = createTeapotGeometry(*pScene);

    // Create matrix and material for first instance (which is linearized).
    const Path kMaterialPath = "NormalMaterial";
    pScene->setMaterialProperties(kMaterialPath, { { "normal_image", loadImage(txtName, false) } });
    mat4 scaleMtx = scale(vec3(2, 2, 2));

    // Create geometry with the material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath;
    instProps[Names::InstanceProperties::kTransform] = scaleMtx;
    EXPECT_TRUE(pScene->addInstance(nextPath(), planePath, instProps));

    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath;
    instProps[Names::InstanceProperties::kTransform] = mat4();
    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Images");
}

// Create image immediately after scene creation.
TEST_P(ImageTest, TestCreateImageAfterSceneCreation)
{
    auto pScene = createDefaultScene();

    // If pScene is null this renderer type not supported, skip rest of the test.
    if (!pScene)
        return;

    const Path kImagePath = "DefaultImage";
    ImageDescriptor imageDesc;
    imageDesc.isEnvironment = false;
    imageDesc.linearize     = true;

    // Load pixels for test image file.
    std::vector<unsigned char> buffer(1024 * 2048 * 4);

    bool loaded = false;

    // Set up the pixel data callback
    imageDesc.getData = [&buffer, &loaded](ImageData& dataOut, AllocateBufferFunction alloc) {
        // Get address and size from buffer (assumes will be called from scope of test, so buffer
        // still valid)
        dataOut.pPixelBuffer = buffer.data();
        dataOut.bufferSize   = buffer.size();
        dataOut.dimensions   = { 2048, 1024 };
        loaded               = true;
        return true;
    };

    // Ensure no crash when image created.
    pScene->setImageDescriptor(kImagePath, imageDesc);

    // Not loaded yet as resource not activated.
    ASSERT_FALSE(loaded);

    // Ensure no crash when resource activated by adding permanent ref count.
    ASSERT_NO_FATAL_FAILURE(pScene->addPermanent(kImagePath));

    // Now loaded as resource is activated.
    ASSERT_TRUE(loaded);

    // Remove permanent ref count.
    pScene->removePermanent(kImagePath);
}

INSTANTIATE_TEST_SUITE_P(ImageTests, ImageTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
