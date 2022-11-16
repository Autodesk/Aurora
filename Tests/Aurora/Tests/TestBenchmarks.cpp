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

#if !defined(DISABLE_UNIT_TESTS)

#include <chrono>
#include <cmath>

#include "AuroraTestHelpers.h"
#include "BaselineImageHelpers.h"

using namespace Aurora;

namespace
{

class BenchmarkTest : public TestHelpers::FixtureBase
{
public:
    BenchmarkTest() {}
    ~BenchmarkTest() {}
};

// Basic benchmark test.(skipped by default)
TEST_P(BenchmarkTest, TestBenchmarkDefault)
{
    // Benchmark tests are not run by default.
    if (!TestHelpers::getFlagEnvironmentVariable("ENABLE_BENCHMARK_TESTS"))
        return;

    // Create the default renderer with 1k by 1k frame buffer.
    IRendererPtr pRenderer = createDefaultRenderer(1024, 1024);

    // Create default scene
    Aurora::IScenePtr pScene = createDefaultScene();

    // If pRenderer is null this renderer type not supported, skip rest of the test
    if (!pRenderer)
        return;

    // Quad geometry: positions, normals, and indices.
    // clang-format off
    array<vec3, 4> quadVerts = {
        vec3(-2, -2, 0),
        vec3(+2, -2, 0),
        vec3(-2, +2, 0),
        vec3(+2, +2, 0)
    };
    array<vec3, 4> quadNormals = {
        vec3(0, 0, 1),
        vec3(0, 0, 1),
        vec3(0, 0, 1),
        vec3(0, 0, 1)
    };
    array<uint32_t, 12> quadInd = {
        0, 1, 2,
        3, 2, 1,
        2, 1, 0,
        1, 2, 3,
    };
    // clang-format on

    // Create the quad geometry.
    const Path kDefaultQuadPath = "DefaultQuadGeometry";
    GeometryDescriptor geomDesc;
    geomDesc.type                                                      = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kPosition] = AttributeFormat::Float3;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kNormal]   = AttributeFormat::Float3;
    geomDesc.vertexDesc.count                                          = quadVerts.size();
    geomDesc.indexCount                                                = quadInd.size();
    geomDesc.getAttributeData = [&quadVerts, &quadNormals, &quadInd](AttributeDataMap& buffers,
                                    size_t firstVertex, size_t vertexCount, size_t firstIndex,
                                    size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == quadVerts.size(), "Partial update not supported");

        buffers[Names::VertexAttributes::kPosition].address = quadVerts.data();
        buffers[Names::VertexAttributes::kPosition].size    = quadVerts.size() * sizeof(vec3);
        buffers[Names::VertexAttributes::kPosition].stride  = sizeof(vec3);
        buffers[Names::VertexAttributes::kNormal].address   = quadNormals.data();
        buffers[Names::VertexAttributes::kNormal].size      = quadNormals.size() * sizeof(vec3);
        buffers[Names::VertexAttributes::kNormal].stride    = sizeof(vec3);

        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == quadInd.size(), "Partial update not supported");

        buffers[Names::VertexAttributes::kIndices].address = quadInd.data();
        buffers[Names::VertexAttributes::kIndices].size    = quadInd.size() * sizeof(uint32_t);
        buffers[Names::VertexAttributes::kIndices].stride  = sizeof(uint32_t);

        return true;
    };

    pScene->setGeometryDescriptor(kDefaultQuadPath, geomDesc);

    // Create the quad geometry.
    EXPECT_TRUE(pScene->addInstance(kDefaultQuadPath, nextPath()));

    // Create spiral for foreground quads.
    const size_t kNumInstances = 5;

    for (size_t index = 0; index < kNumInstances; index++)
    {
        // u varies from 0.0 to 1.0.
        float u    = (float)index / (float)kNumInstances;
        vec3 color = { u, 1.0f - u, 1.0f };

        // Create a new material.
        Path matPath = nextPath("Material");
        pScene->setMaterialProperties(matPath, { { "base_color", color } });

        // Set the quad's position within spiral.
        float theta    = static_cast<float>(u * 2.0f * M_PI);
        vec3 spiralPos = vec3(cosf(theta), sinf(theta), -0.5f - u * 0.4f);

        // Create transform from position and scale.
        mat4 xform = translate(spiralPos) * scale(vec3(0.3f, 0.3f, 0.3f));

        // Add a new instance.
        Properties instProps;
        instProps[Names::InstanceProperties::kMaterial]  = matPath;
        instProps[Names::InstanceProperties::kTransform] = xform;
        EXPECT_TRUE(pScene->addInstance(kDefaultQuadPath, nextPath(), instProps));
    }

    // Render a warm-up frame, and wait for it to complete, so the setup cost for first frame not
    // included in the benchmark timing.
    pRenderer->render();
    pRenderer->waitForTask();

    // Total number of samples is samples*numIter.
    const int samples = 16;
    const int numIter = 10;

    // Start sample position for each iteration.
    int startSample = 0;

    // Start timing the iterations.
    auto start = std::chrono::system_clock::now();

    // Run the iterations
    for (int i = 0; i < numIter; i++)
    {
        pRenderer->render(startSample, samples);
        pRenderer->waitForTask();
        startSample += samples;
    }

    // Finish timing and print results.
    std::chrono::system_clock::time_point end     = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << samples * numIter << " total samples took " << elapsed_seconds.count() << "s\n";

    // Ensure rendered image correct using baseline image test.
    // NOTE: as image so large (and test not run by default) this is not actual checked in.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

INSTANTIATE_TEST_SUITE_P(BenchmarkTests, BenchmarkTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
