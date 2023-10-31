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

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct GeomData
{
    vector<Vertex> verts;
    vector<uint32_t> indices;
};

class PathTest : public TestHelpers::FixtureBase
{
public:
    PathTest() {}
    ~PathTest() {}

    GetImageDataFunction createImageFunction(
        int w, int h, int s, vec3 color0, vec3 color1, string name)
    {
        _imageFunctionInvocations[name] = 0;
        return [this, w, h, s, color0, color1, name](
                   ImageData& dataOut, AllocateBufferFunction alloc) {
            uint8_t* pPixelData = static_cast<uint8_t*>(alloc(w * h * 4));
            int idx             = 0;
            for (int i = 0; i < w; i++)
            {
                for (int j = 0; j < h; j++)
                {
                    vec3 color        = ((i / s) % 2 == (j / s) % 2) ? color0 : color1;
                    pPixelData[idx++] = static_cast<uint8_t>(color.x * 255.0f);
                    pPixelData[idx++] = static_cast<uint8_t>(color.y * 255.0f);
                    pPixelData[idx++] = static_cast<uint8_t>(color.z * 255.0f);
                    pPixelData[idx++] = 255;
                }
            }
            dataOut.pPixelBuffer = pPixelData;
            dataOut.bufferSize   = w * h * 4;
            dataOut.dimensions   = { w, h };
            dataOut.format       = Aurora::ImageFormat::Integer_RGBA;
            _imageFunctionInvocations[name]++;
            return true;
        };
    }

    map<string, int> _imageFunctionInvocations;
    unique_ptr<GeomData> _pGeometryDataP;
};

// Basic path test.
TEST_P(PathTest, TestPathDefault)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create the first image.
    const Path kImagePath0 = nextPath("Image");
    ImageDescriptor imageDesc0;
    imageDesc0.linearize = true;
    imageDesc0.getData   = createImageFunction(4, 4, 2, vec3(1, 0, 0), vec3(0, 1, 0), kImagePath0);
    pScene->setImageDescriptor(kImagePath0, imageDesc0);

    // Create the second simage.
    const Path kImagePath1 = nextPath("Image");
    ImageDescriptor imageDesc1;
    imageDesc1.linearize = true;
    imageDesc1.getData =
        createImageFunction(8, 8, 2, vec3(0, 1, 0.8), vec3(0.2, 0.0, 0.75), kImagePath1);
    pScene->setImageDescriptor(kImagePath1, imageDesc1);

    // Create the third image.
    const Path kImagePath2 = nextPath("Image");
    ImageDescriptor imageDesc2;
    imageDesc2.linearize = true;
    imageDesc2.getData = createImageFunction(16, 16, 4, vec3(0, 0, 0), vec3(1, 1, 1), kImagePath2);
    pScene->setImageDescriptor(kImagePath2, imageDesc2);

    // Pixel data yet, only created when activated.
    // No renderer data has been created yet, nothing has been activated.
    ASSERT_EQ(_imageFunctionInvocations[kImagePath0], 0);

    // Create first material.
    const Path kMaterialPath0 = nextPath("Material0");
    pScene->setMaterialProperties(kMaterialPath0, { { "base_color_image", kImagePath0 } });

    // Create second material.
    const Path kMaterialPath1 = nextPath("Material1");
    pScene->setMaterialProperties(kMaterialPath1, { { "base_color_image", kImagePath1 } });

    // Create plane instance.
    Path geomPath = createPlaneGeometry(*pScene);

    // No renderer data has been created yet, nothing has been activated.
    ASSERT_EQ(_imageFunctionInvocations[kImagePath0], 0);

    // Create instance with material.
    Paths instPaths = pScene->addInstances(geomPath,
        { { nextPath("Instance"),
              { { Names::InstanceProperties::kMaterial, kMaterialPath0 },
                  { Names::InstanceProperties::kTransform, translate(glm::vec3(-2.5, 0, +4)) } } },
            { nextPath("Instance"),
                { { Names::InstanceProperties::kMaterial, kMaterialPath0 },
                    { Names::InstanceProperties::kTransform, translate(glm::vec3(0.0, 0, +4)) } } },
            { nextPath("Instance"),
                { { Names::InstanceProperties::kMaterial, kMaterialPath0 },
                    { Names::InstanceProperties::kTransform,
                        translate(glm::vec3(+2.5, 0, +4)) } } } });

    ASSERT_EQ(instPaths.size(), 3);

    // Now the image getData function should have been invoced once.
    ASSERT_EQ(_imageFunctionInvocations[kImagePath0], 1);

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "0", "Paths");

    pScene->setInstanceProperties(
        instPaths[1], { { Names::InstanceProperties::kMaterial, kMaterialPath1 } });

    // Now the image getData function should have been invoced for first two images.
    ASSERT_EQ(_imageFunctionInvocations[kImagePath0], 1);
    ASSERT_EQ(_imageFunctionInvocations[kImagePath1], 1);

    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "1", "Paths");

    pScene->setMaterialProperties(kMaterialPath0, { { "base_color_image", kImagePath2 } });

    // Now the image getData function should have been invoced for all the images.
    ASSERT_EQ(_imageFunctionInvocations[kImagePath0], 1);
    ASSERT_EQ(_imageFunctionInvocations[kImagePath1], 1);
    ASSERT_EQ(_imageFunctionInvocations[kImagePath2], 1);

    ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(currentTestName() + "2", "Paths");
}

INSTANTIATE_TEST_SUITE_P(PathTests, PathTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
