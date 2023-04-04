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

#include <array>
#include <chrono>
#include <cmath>

#include "AuroraTestHelpers.h"
#include "BaselineImageHelpers.h"

#include <fstream>

using namespace Aurora;

namespace
{

class LightTest : public TestHelpers::FixtureBase
{
public:
    LightTest() {}
    ~LightTest() {}
};

// Create test HDR lat-long environment map image data, interpolated from colors for on six
// directions (+/- X, +/- Y, +/- Z)
// If lightIntensity non-zero bright region added around lightDirec vector.
void createTestEnv(IScenePtr scene, const Path& path, int height, const array<glm::vec3, 6>& colors,
    glm::vec3 lightDirection, glm::vec3 lightColor, float lightAngle, float lightIntensity,
    std::vector<unsigned char>* pBufferOut, bool isRGBA)
{
    // Width is always 2x height for lat-long.
    int width = height * 2;

    // Allocate data for image.
    int numComp = isRGBA ? 4 : 3;
    pBufferOut->resize(width * height * sizeof(float) * numComp);

    // Get float pixels for image.
    float* pPixels = (float*)pBufferOut->data();

    float lightAngleRad = glm::radians(lightAngle);

    // Iterate through rows of image.
    for (int row = 0; row < height; row++)
    {
        // Compute Phi angle in radians from row.
        float v   = (float)row / (float)(height - 1);
        float phi = v * (float)M_PI;

        // Iterate through columns.
        for (int col = 0; col < width; col++)
        {
            // Compute Theta angle in radians from column.
            float u     = (float)col / (float)(width - 1);
            float theta = u * 2.0f * (float)M_PI;

            // Compute direction from spherical coordinate.
            glm::vec3 dir(sinf(theta) * sinf(phi), cosf(phi), cosf(theta) * sinf(phi));

            // Compute color for each axis in direction.
            glm::vec3 color;
            color += dir.x > 0.0 ? colors[0] * dir.x : colors[1] * -1.0f * dir.x;
            color += dir.y > 0.0 ? colors[2] * dir.y : colors[3] * -1.0f * dir.y;
            color += dir.z > 0.0 ? colors[4] * dir.z : colors[5] * -1.0f * dir.z;

            if (lightIntensity > 0.0f)
            {
                float currLightAngleRad = acosf(glm::dot(dir, normalize(lightDirection)));
                if (currLightAngleRad < lightAngleRad)
                {
                    color = lightColor * lightIntensity;
                }
            }

            // Put into pixels array.
            *(pPixels++) = color.x;
            *(pPixels++) = color.y;
            *(pPixels++) = color.z;

            // Add alpha component if needed.
            if (isRGBA)
                *(pPixels++) = 1.0f;
        }
    }

    // Create and return Aurora Environment Map Path.
    ImageDescriptor imageDesc;
    imageDesc.width         = width;
    imageDesc.height        = height;
    imageDesc.format        = isRGBA ? ImageFormat::Float_RGBA : ImageFormat::Float_RGB;
    imageDesc.isEnvironment = true;
    imageDesc.linearize     = true;
    imageDesc.getPixelData  = [pBufferOut](PixelData& dataOut, glm::ivec2, glm::ivec2) {
        // Get addres and size from buffer (assumes will be called from scope of test, so buffer
        // still valid)
        dataOut.address = pBufferOut->data();
        dataOut.size    = pBufferOut->size();
        return true;
    };

    scene->setImageDescriptor(path, imageDesc);
}

// Basic environment map image test.
TEST_P(LightTest, TestLightEnvTexture)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Disable the directional light.
    vec3 lightDirec(0, 0, 1);
    rgb lightColor(0, 0, 0);
    pScene->setLight(0.0, lightColor, lightDirec);

    // Create procedural image data.
    std::vector<unsigned char> buffer;
    array<glm::vec3, 6> colors = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.75f, 1.0f),
        glm::vec3(0.75f, 0.0f, 1.0f),
        glm::vec3(0.8f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.8f, 0.0f),
    };
    const Path kBackgroundEnvironmentImagePath = "BackgroundEnvironmentImage";
    createTestEnv(pScene, kBackgroundEnvironmentImagePath, 512, colors, glm::vec3(), glm::vec3(), 0,
        0, &buffer, false);

    // Create environment and set background and light image.
    const Path kBackgroundEnvironmentPath = "BackgroundEnvironment";
    pScene->setEnvironmentProperties(kBackgroundEnvironmentPath,
        {
            { Names::EnvironmentProperties::kLightImage, kBackgroundEnvironmentImagePath },
            { Names::EnvironmentProperties::kBackgroundImage, kBackgroundEnvironmentImagePath },
        });

    // Set the environment.
    pScene->setEnvironment(kBackgroundEnvironmentPath);

    // Create a material.
    const Path kMaterialPath = "DefaultMaterial";
    pScene->setMaterialType(kMaterialPath);

    // Create teapot instance.
    Path GeomPath = createTeapotGeometry(*pScene);

    // Create instance with material.
    EXPECT_TRUE(pScene->addInstance(
        nextPath(), GeomPath, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Light");
}

// Basic environment map image test.
TEST_P(LightTest, TestLightEnvTextureMIS)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();
    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Set wider FOV so both teapots visible.
    setDefaultRendererPerspective(60.0f);

    // Disable the directional light.
    vec3 lightDirec(0, 0, 1);
    rgb lightColor(0, 0, 0);
    pScene->setLight(0.0, lightColor, lightDirec);

    // Create procedural image data with small bright region.
    std::vector<unsigned char> buffer;
    array<glm::vec3, 6> colors = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.75f, 1.0f),
        glm::vec3(0.75f, 0.0f, 1.0f),
        glm::vec3(0.8f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.8f, 0.0f),
    };
    const Path kBackgroundEnvironmentImagePath = "BackgroundEnvironmentImage";
    createTestEnv(pScene, kBackgroundEnvironmentImagePath, 1024, colors, glm::vec3(0, 0.2f, 1),
        glm::vec3(0.9f, 0.8f, -0.8f), 0.5f, 5000.0f, &buffer, false);

    // Create environment and set background and light image.
    const Path kBackgroundEnvironmentPath = "BackgroundEnvironment";
    pScene->setEnvironmentProperties(kBackgroundEnvironmentPath,
        {
            { Names::EnvironmentProperties::kLightImage, kBackgroundEnvironmentImagePath },
            { Names::EnvironmentProperties::kBackgroundImage, kBackgroundEnvironmentImagePath },
        });

    // Set the environment.
    pScene->setEnvironment(kBackgroundEnvironmentPath);

    // Create geometry.
    Path teapotPath = createTeapotGeometry(*pScene);
    Path planePath  = createPlaneGeometry(*pScene);

    // Create one glossy and one diffuse material
    const Path kMaterialPath0 = "Material0";
    pScene->setMaterialProperties(kMaterialPath0, { { "specular_roughness", 0.95f } });
    const Path kMaterialPath1 = "Material1";
    pScene->setMaterialProperties(kMaterialPath1, { { "specular_roughness", 0.05f } });

    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath,
        { { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(+3, -1.5, 3)) },
            { Names::InstanceProperties::kMaterial, kMaterialPath0 } }));
    EXPECT_TRUE(pScene->addInstance(nextPath(), planePath,
        { { Names::InstanceProperties::kTransform,
              glm::translate(glm::vec3(+50, -1.5, 0)) *
                  glm::rotate(static_cast<float>(M_PI * 0.5), glm::vec3(1, 0, 0)) *
                  glm::scale(glm::vec3(50, 50, 50)) },
            { Names::InstanceProperties::kMaterial, kMaterialPath1 } }));

    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath,
        { { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(-3, -1.5, 3)) },
            { Names::InstanceProperties::kMaterial, kMaterialPath0 } }));
    EXPECT_TRUE(pScene->addInstance(nextPath(), planePath,
        { { Names::InstanceProperties::kTransform,
              glm::translate(glm::vec3(-50, -1.5, 0)) *
                  glm::rotate(static_cast<float>(M_PI * 0.5), glm::vec3(1, 0, 0)) *
                  glm::scale(glm::vec3(50, 50, 50)) },
            { Names::InstanceProperties::kMaterial, kMaterialPath1 } }));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName(), "Light");
}

INSTANTIATE_TEST_SUITE_P(LightTests, LightTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
