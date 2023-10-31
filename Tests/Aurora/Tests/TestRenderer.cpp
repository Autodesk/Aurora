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

#include <cmath>

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

class RendererTest : public TestHelpers::FixtureBase
{
public:
    RendererTest() {}
    ~RendererTest() {}
};

// Test basic renderer functionality.
TEST_P(RendererTest, TestRendererDefault)
{
    // TODO: Should test assert message when creating renderer when not supported.
    if (!backendSupported())
        return;

    // Create a renderer for this type.
    IRenderer::Backend backend = rendererBackend();
    IRendererPtr pRenderer     = createRenderer(backend);

    // Expect a valid renderer.
    ASSERT_NE(pRenderer, nullptr) << rendererDescription();
    ASSERT_EQ(pRenderer->backend(), backend) << rendererDescription();
}

// Ensure debug flag is not set on device.
TEST_P(RendererTest, TestRendererDebugDevice)
{
    if (!backendSupported())
        return;

    auto renderer = createRenderer(rendererBackend());
    ASSERT_NE(renderer.get(), nullptr);

    // Look for the string that DXDevice::initialize will print to console if debug enabled.
    ASSERT_THAT(lastLogMessage(), ::testing::Not(::testing::HasSubstr("AU_DEVICE_DEBUG_ENABLED")));
}

// Test creating, destroying renderer then rendering.
TEST_P(RendererTest, TestRendererCreateDestroyThenRenderFull)
{

    // TODO: Should test assert message when creating renderer when not supported.
    if (!backendSupported())
        return;

    // Create and destroy renderer (both types) three times to ensure we aren't leaking resources
    {
        auto r1 = createRenderer(rendererBackend());
        r1.reset();
    }
    {
        auto r1 = createRenderer(rendererBackend());
        r1.reset();
    }
    {
        auto r1 = createRenderer(rendererBackend());
        r1.reset();
    }

    // Create the default scene and render it
    IScenePtr pScene       = createDefaultScene();
    IRendererPtr pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test
    if (!pRenderer)
        return;

    // Create a teapot instance with default material
    Path geometry = createTeapotGeometry(*pScene);

    // Add to scene.
    Path instance("CreateDestroyInstance");
    Properties instProps;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Ensure result correct using baseline image test
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Test renderer options.
TEST_P(RendererTest, TestRendererOptions)
{
    if (!backendSupported())
        return;

    // Create a renderer for this type.
    IRenderer::Backend backend = rendererBackend();
    IRendererPtr pRenderer     = createRenderer(backend);

    // Test for the existence of renderer options.
    // NOTE: Currently all of these are DirectX only, some should probably exist in
    // rasterization.
    testFloat3Option(*pRenderer, "brightness", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testFloatOption(*pRenderer, "maxLuminance", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testBooleanOption(*pRenderer, "isDiffuseOnlyEnabled", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testIntOption(*pRenderer, "debugMode", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testBooleanOption(*pRenderer, "isToneMappingEnabled", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testBooleanOption(*pRenderer, "isGammaCorrectionEnabled", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");
    testIntOption(*pRenderer, "traceDepth", true,
        " option test line " + std::to_string(__LINE__) + " (" + rendererDescription() + ")");

    // Test for non-existent option
    testFloat3Option(*pRenderer, "fooooo!", false,
        " non-existent option test line " + std::to_string(__LINE__) + " (" +
            rendererDescription() + ")");
}

// Render with increasing values of sampleCount.
// Should have no effect on rasterizer at all
TEST_P(RendererTest, TestRendererLargeSampleCount)
{

    // Create the default scene with 256x256 frame size (also creates renderer)
    IRendererPtr pRenderer = createDefaultRenderer(256, 256);
    IScenePtr pScene       = createDefaultScene();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a grid of teapot instances, each with its own material.
    const uint32_t gridWidth  = 5;
    const uint32_t gridHeight = 5;
    std::array<Path, gridWidth * gridHeight> materialGrid;

    // Create shared geom for all instances.
    Path geometry = createTeapotGeometry(*pScene);

    // Create grid parameters.
    vec3 gridDims(gridWidth - 1, gridHeight - 1, 0.0f);
    vec3 gridOrigin(0, 0, +17.0f);
    vec3 gridGap(3.1f, 3.1f, 0.0f);
    vec3 gridHalfSize = gridGap * gridDims * 0.5f;

    // Create a grid of teapots.
    for (uint32_t i = 0; i < gridHeight; i++)
    {
        for (uint32_t j = 0; j < gridWidth; j++)
        {
            // Calculate transform matrix from grid position
            vec3 gridPos = vec3(i, j, 0) * gridGap + gridOrigin - gridHalfSize;
            mat4 xform   = translate(gridPos);

            // Create material for grid
            size_t index  = i * size_t(gridWidth) + j;
            Path material = "LargeSampleMaterial" + to_string(index);

            // Set the default material type to trigger creation of material.
            pScene->setMaterialType(material);
            materialGrid[index] = material;

            Path instance = "LargeSampleInstance" + to_string(index);
            Properties instProps;
            instProps[Names::InstanceProperties::kMaterial]  = material;
            instProps[Names::InstanceProperties::kTransform] = xform;
            EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));
        }
    }

    // Render with increasing sample counts
    for (uint32_t i = 0; i < 10; i++)
    {
        // Render with sample count
        uint32_t sampleCount = 16 + i * 200;
        pRenderer->render(0, sampleCount);
    }
}

// Test destroying render buffer after renderer.
TEST_P(RendererTest, TestRendererDestroyRenderBufferFirst)
{
    // TODO: Should test assert message when creating renderer when not supported.
    if (!backendSupported())
        return;

    // Not currently supported on Vulkan.
    if (!isDirectX())
        return;

    // Create a renderer for this type.
    IRenderer::Backend backend = rendererBackend();
    IRendererPtr pRenderer     = createRenderer(backend);

    // Create a default render buffer.
    IRenderBufferPtr pRenderBuffer =
        pRenderer->createRenderBuffer(16, 16, ImageFormat::Integer_RGBA);
    pRenderer->setTargets({ { AOV::kFinal, pRenderBuffer } });

    // Create a scene.
    IScenePtr pScene = pRenderer->createScene();

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    // Set the scene in the renderer.
    pRenderer->setScene(pScene);

    // Render should succeed.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());

    // Wait for frame, to ensure render buffer not in use
    pRenderer->waitForTask();

    // Change the render buffer format and re-render.
    pRenderBuffer = pRenderer->createRenderBuffer(16, 16, ImageFormat::Integer_RGBA);
    pRenderer->setTargets({ { AOV::kFinal, pRenderBuffer } });

    // Render should succeed.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());

    // Ordering doesn't matter for PT.
    ASSERT_NO_FATAL_FAILURE(pRenderer.reset());
    ASSERT_NO_FATAL_FAILURE(pScene.reset());
    ASSERT_NO_FATAL_FAILURE(pRenderBuffer.reset());
}

// Test a completely empty scene.
TEST_P(RendererTest, TestRendererEmptyScene)
{
    // Create the default scene. (also creates renderer)
    IRendererPtr pRenderer = createDefaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a scene.
    IScenePtr pScene = pRenderer->createScene();

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    // Don't create any geometry completely empty scene.

    // Set the scene in the renderer.
    pRenderer->setScene(pScene);

    // Render should succeed.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());
}

// Test background the same between two identical renders
TEST_P(RendererTest, TestRenderBackgroundTheSameBetweenRenders)
{
    // Create the default scene (also creates renderer)
    IScenePtr pScene       = createDefaultScene();
    IRendererPtr pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a teapot instance with default material
    Path geometry = createTeapotGeometry(*pScene);

    // Add to scene.
    Path instance("RenderBackgroundInstance");
    EXPECT_TRUE(pScene->addInstance(instance, geometry));

    // Render the first frame.
    pRenderer->render(0, 16);

    // Get first pixel after first frame.
    size_t stride;
    const uint8_t* renderedImage0 =
        reinterpret_cast<const uint8_t*>(defaultRenderBuffer()->data(stride, true));
    ASSERT_NE(renderedImage0, nullptr);
    uint8_t firstPixelFirstRender[] = { renderedImage0[0], renderedImage0[1], renderedImage0[2],
        renderedImage0[3] };

    // Render first frame.
    pRenderer->render(0, 16);

    // Get first pixel after second frame.
    const uint8_t* renderedImage1 =
        reinterpret_cast<const uint8_t*>(defaultRenderBuffer()->data(stride, true));
    ASSERT_NE(renderedImage0, nullptr);
    uint8_t firstPixelSecondRender[] = { renderedImage1[0], renderedImage1[1], renderedImage1[2],
        renderedImage1[3] };

    // Use a byte value of 5 as max acceptable difference between renders
    uint8_t maxDiff = 5;

    // Ensure RGB of first pixel of image has not changed significantly
    ASSERT_LT(std::abs(firstPixelFirstRender[0] - firstPixelSecondRender[0]), maxDiff);
    ASSERT_LT(std::abs(firstPixelFirstRender[1] - firstPixelSecondRender[1]), maxDiff);
    ASSERT_LT(std::abs(firstPixelFirstRender[2] - firstPixelSecondRender[2]), maxDiff);

    // Alpha should always be 255
    ASSERT_EQ(firstPixelFirstRender[3], 255);
    ASSERT_EQ(firstPixelSecondRender[3], 255);
}

// Test background the same between two identical renders
TEST_P(RendererTest, TestRenderReadableBuffer)
{
    // No layers on HGI currently.
    if (!isDirectX())
        return;

    // Create the default scene (also creates renderer)
    IScenePtr pScene       = createDefaultScene();
    IRendererPtr pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a teapot instance with default material
    Path geometry = createTeapotGeometry(*pScene);

    // Add to scene.
    Path instance("ReadableBufferInstance");
    EXPECT_TRUE(pScene->addInstance(instance, geometry));

    // Render the first frame.
    pRenderer->render(0, 16);

    size_t stride;
    IRenderBuffer::IBufferPtr readableBuffer1 = defaultRenderBuffer()->asReadable(stride);

    // Ensure that the readable buffer has valid data.
    const uint8_t* renderedImage0 = static_cast<const uint8_t*>(readableBuffer1->data());
    ASSERT_NE(renderedImage0, nullptr);
    uint8_t firstPixelFirstRender[] = { renderedImage0[0], renderedImage0[1], renderedImage0[2],
        renderedImage0[3] };

    // Render second frame.  Ensure we can render with an already mapped render buffer?
    pRenderer->render(0, 16);

    // Call as Readable again without releasing the first one.
    IRenderBuffer::IBufferPtr readableBuffer2 = defaultRenderBuffer()->asReadable(stride);

    // Ensure that the readable buffer has valid data.
    const uint8_t* renderedImage1 = static_cast<const uint8_t*>(readableBuffer2->data());
    ASSERT_NE(renderedImage1, nullptr);
    uint8_t firstPixelSecondRender[] = { renderedImage1[0], renderedImage1[1], renderedImage1[2],
        renderedImage1[3] };

    // Use a byte value of 5 as max acceptable difference between renders
    uint8_t maxDiff = 5;

    // Ensure RGB of first pixel of image has not changed significantly
    ASSERT_LT(std::abs(firstPixelFirstRender[0] - firstPixelSecondRender[0]), maxDiff);
    ASSERT_LT(std::abs(firstPixelFirstRender[1] - firstPixelSecondRender[1]), maxDiff);
    ASSERT_LT(std::abs(firstPixelFirstRender[2] - firstPixelSecondRender[2]), maxDiff);

    // Alpha should always be 255
    ASSERT_EQ(firstPixelFirstRender[3], 255);
    ASSERT_EQ(firstPixelSecondRender[3], 255);
}

// Test zero volume scene bounds.
// TODO: Disabled as causing CI/CD errors.
TEST_P(RendererTest, DISABLED_TestRendererInvalidBounds)
{

    // Create the default scene and renderer.
    Aurora::IRendererPtr pRenderer = createDefaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a scene.
    Aurora::IScenePtr pScene = pRenderer->createScene();

    // Create a teapot instance with default material
    Path geometry = createTeapotGeometry(*pScene);

    // Add to scene.
    Path instance("RendererInvalidBoundsInstance");
    EXPECT_TRUE(pScene->addInstance(instance, geometry));

    // Set invalid zero-volume bounds.
    vec3 boundsMin(0.0, 0.0, 0.0);
    vec3 boundsMax(0.0, 0.0, 0.0);
    pScene->setBounds(boundsMin, boundsMax);

    // Set the scene in the renderer.
    pRenderer->setScene(pScene);

    // Ensure exception thrown.
    ASSERT_THROW(pRenderer->render(), TestHelpers::AuroraLoggerException);
    ASSERT_THAT(lastLogMessage(),
        ::testing::StartsWith("AU_ASSERT test failed:\nEXPRESSION: _pScene->bounds().isValid()"));
}

// Ensure can render empty scene with no bounds.
TEST_P(RendererTest, TestRendererEmptySceneBounds)
{

    // Create the default scene and renderer.
    Aurora::IRendererPtr pRenderer = createDefaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a scene and don't set the bounds
    Aurora::IScenePtr pScene = pRenderer->createScene();

    // Set the scene in the renderer.
    pRenderer->setScene(pScene);

    // Ensure exception thrown.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());
}

// Test ground plane.
// TODO: Re-enable once ground plane re-enabled.
TEST_P(RendererTest, TestRendererGroundPlane)
{
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    Aurora::IGroundPlanePtr pGroundPlane = defaultRenderer()->createGroundPlanePointer();
    pGroundPlane->values().setBoolean("enabled", true);
    pGroundPlane->values().setFloat3("position", value_ptr(vec3(0, 0, 0)));
    pGroundPlane->values().setFloat3("normal", value_ptr(vec3(0, 1, 0)));
    pGroundPlane->values().setFloat("shadow_opacity", 1.0f);
    pGroundPlane->values().setFloat("reflection_opacity", 1.0f);
    pScene->setGroundPlanePointer(pGroundPlane);

    setDefaultRendererCamera(vec3(0, 1, -5), vec3(0, 0, 0));

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    Path kMaterialPath = "MaterialPath";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color", vec3(1, 0, 0) } });

    // Create a teapot instance with a default material.
    Path geometry = createTeapotGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    // Ensure result is correct by comparing to the baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Test null environment.
TEST_P(RendererTest, TestRendererSetNullEnvironment)
{

    // Create the default scene and renderer.
    IRendererPtr pRenderer = createDefaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create a scene.
    IScenePtr pScene = pRenderer->createScene();

    // Set arbritrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    // Set the scene in the renderer.
    pRenderer->setScene(pScene);

    // Set a null environment.
    pScene->setEnvironment(Path());

    // All implementations should support null environment.
    ASSERT_NO_FATAL_FAILURE(pRenderer->render());
}

// Test instance properties environment.
TEST_P(RendererTest, TestRendererInstanceProperties)
{
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    Path kMaterialPath = "MaterialPath";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color", vec3(1, 0, 0) } });

    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    Path planeGeometry = createPlaneGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));
    EXPECT_TRUE(pScene->addInstance(
        "Plane", planeGeometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    // Hide the teapot.
    pScene->setInstanceProperties(
        "TeapotInstance", { { Names::InstanceProperties::kVisible, false } });

    // Ensure result is correct by comparing to the baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Test non-indexed geom
TEST_P(RendererTest, TestRendererNonIndexedGeom)
{
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    Path kMaterialPath = "MaterialPath";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color", vec3(1, 0, 0) } });

    // Triangle geometry and vertex count.
    size_t vertexCount = 3;
    // clang-format off
    float positions[] = {
         0.0f,   0.0f, 0.0f,
         2.5f, -5.0f, 0.0f,
        -2.5f, -5.0f, 0.0f,
    };
    float normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };

    // Create a non-indexed triangle.
    Path geomPath = nextPath("NonIndexedGeom");
    GeometryDescriptor geomDesc;
    geomDesc.type                                                       = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kPosition]  = AttributeFormat::Float3;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kNormal]    = AttributeFormat::Float3;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
    geomDesc.vertexDesc.count = vertexCount;
    geomDesc.indexCount       = 0;
    geomDesc.getAttributeData = [&](AttributeDataMap& buffers, size_t firstVertex, size_t vc,
                                 size_t /*firstIndex*/, size_t /*indexCount*/) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == vc, "Partial update not supported");
        buffers[Names::VertexAttributes::kPosition].address = positions;
        buffers[Names::VertexAttributes::kPosition].size =vc * sizeof(vec3);
        buffers[Names::VertexAttributes::kPosition].stride = sizeof(vec3);
        buffers[Names::VertexAttributes::kNormal].address  = normals;
        buffers[Names::VertexAttributes::kNormal].size = vc * sizeof(vec3);
        buffers[Names::VertexAttributes::kNormal].stride     = sizeof(vec3);
        return true;
    };
    pScene->setGeometryDescriptor(geomPath, geomDesc);

    // Create a teapot instance with a default material.
    EXPECT_TRUE(pScene->addInstance(
        "NonIndexedInstance", geomPath, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());

}

// Test remove instance.
TEST_P(RendererTest, TestRendererRemoveInstance)
{
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Set arbitrary bounds.
    vec3 boundsMin(-1, -1, -1);
    vec3 boundsMax(+1, +1, +1);
    pScene->setBounds(boundsMin, boundsMax);

    Path kMaterialPath = "MaterialPath";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color", vec3(1, 0, 0) } });

    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    Path planeGeometry = createPlaneGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));
    EXPECT_TRUE(pScene->addInstance(
        "Plane", planeGeometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName() + "Both");

    // Remove the teapot.
    pScene->removeInstance("TeapotInstance");

    // Ensure result is correct by comparing to the baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName()+"TeapotRemoved");
}

// Test instance with layer materials
// TODO: Re-enable test when layers are working.
TEST_P(RendererTest, TestRendererMaterialLayers)
{
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create the image.
    const std::string txtNormalName = dataPath() + "/Textures/fishscale_normal.png";
    const Path normalImagePath = loadImage(txtNormalName, false);

    Path kMaterialPath = "BaseMaterial";
    pScene->setMaterialProperties(kMaterialPath, {{"normal_image", normalImagePath}, { "base_color", vec3(1, 0, 0) } });


    // Load opacity image file.
    const std::string txtName = dataPath() +
        "/Textures/Triangle.png";
    Path opacityImagePath = loadImage(txtName);

    // Load diffuse image file.
    const std::string mandrillTxPath = dataPath() + "/Textures/Mandrill.png";
    Path mandrillImagePath = loadImage(mandrillTxPath);
    
    // Create a material.
    const Path kDecalMaterialPath0 = "kDecalMaterial0";
    pScene->setMaterialProperties(kDecalMaterialPath0, { { "opacity_image", opacityImagePath }, { "base_color_image", mandrillImagePath }, {"diffuse_roughness",0.9f} });

    ;
    vector<glm::vec2> xformedUVs(TestHelpers::TeapotModel::verticesCount());
    for (uint32_t i = 0; i < xformedUVs.size(); i++) {
        xformedUVs[i].x = *(TestHelpers::TeapotModel::uvs()+(i*2+0));
        xformedUVs[i].y = *(TestHelpers::TeapotModel::uvs()+(i*2+1));
        xformedUVs[i]*=0.25;
        xformedUVs[i]+=0.5;
    }

    const Path kDecalUVGeomPath0 = "DecalUVGeomPath0";
    GeometryDescriptor geomDesc;
    geomDesc.type                                                       = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
    geomDesc.vertexDesc.count = TestHelpers::TeapotModel::verticesCount();
    geomDesc.getAttributeData = [&xformedUVs](AttributeDataMap& buffers, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == TestHelpers::TeapotModel::verticesCount(), "Partial update not supported");
        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == 0, "Partial update not supported");
        buffers[Names::VertexAttributes::kTexCoord0].address = &xformedUVs[0];
        buffers[Names::VertexAttributes::kTexCoord0].size =
            TestHelpers::TeapotModel::verticesCount() * sizeof(vec2);
        buffers[Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

        return true;
    };
    pScene->setGeometryDescriptor(kDecalUVGeomPath0, geomDesc);

    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    Path planeGeometry = createPlaneGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, {
            { Names::InstanceProperties::kMaterial, kMaterialPath },
            { Names::InstanceProperties::kMaterialLayers, vector<string>({kDecalMaterialPath0}) },
            { Names::InstanceProperties::kGeometryLayers, vector<string>({kDecalUVGeomPath0}) },
             { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) }
        }));
    EXPECT_TRUE(pScene->addInstance(
        "Plane", planeGeometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Test instance with invalid layer material paths
// TODO: Re-enable test when layers are working.
TEST_P(RendererTest, TestRendererInvalidMaterialLayerPaths)
{
    // No layers on HGI currently.
    if (!isDirectX())
        return;

    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Create the image.
    const std::string txtNormalName = dataPath() + "/Textures/fishscale_normal.png";
    const Path normalImagePath = loadImage(txtNormalName, false);

    Path kMaterialPath = "BaseMaterial";
    pScene->setMaterialProperties(kMaterialPath, {{"normal_image", normalImagePath}, { "base_color", vec3(1, 0, 0) } });


    // Load opacity image.
    const std::string txtName = dataPath() +
        "/Textures/Triangle.png";
    const Path opacityImagePath = loadImage(txtName);

    // Load diffuse image.
    const std::string mandrillTxPath = dataPath() + "/Textures/Mandrill.png";
    const Path mandrillImagePath = loadImage(mandrillTxPath);

    // Create a material.
    const Path kDecalMaterialPath0 = "kDecalMaterial0";
    pScene->setMaterialProperties(kDecalMaterialPath0, { { "opacity_image", opacityImagePath }, { "base_color_image", mandrillImagePath }, {"diffuse_roughness",0.9f} });

    ;
    vector<glm::vec2> xformedUVs(TestHelpers::TeapotModel::verticesCount());
    for (uint32_t i = 0; i < xformedUVs.size(); i++) {
        xformedUVs[i].x = *(TestHelpers::TeapotModel::uvs()+(i*2+0));
        xformedUVs[i].y = *(TestHelpers::TeapotModel::uvs()+(i*2+1));
        xformedUVs[i]*=0.25;
        xformedUVs[i]+=0.5;
    }

    const Path kDecalUVGeomPath0 = "DecalUVGeomPath0";
    GeometryDescriptor geomDesc;
    geomDesc.type                                                       = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
    geomDesc.vertexDesc.count = TestHelpers::TeapotModel::verticesCount();
    geomDesc.getAttributeData = [&xformedUVs](AttributeDataMap& buffers, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == TestHelpers::TeapotModel::verticesCount(), "Partial update not supported");
        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == 0, "Partial update not supported");
        buffers[Names::VertexAttributes::kTexCoord0].address = &xformedUVs[0];
        buffers[Names::VertexAttributes::kTexCoord0].size =
            TestHelpers::TeapotModel::verticesCount() * sizeof(vec2);
        buffers[Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

        return true;
    };
    pScene->setGeometryDescriptor(kDecalUVGeomPath0, geomDesc);

    Path kInvalidPath = "NotAPath.....! :)";

    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    ASSERT_THROW(pScene->addInstance(
        "TeapotInstanceMissingMtl", geometry, {
            { Names::InstanceProperties::kMaterial, kMaterialPath },
            { Names::InstanceProperties::kMaterialLayers, vector<string>({kInvalidPath}) },
            { Names::InstanceProperties::kGeometryLayers, vector<string>({kDecalUVGeomPath0}) },
             { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) }
        }), TestHelpers::AuroraLoggerException);
    ASSERT_THAT(lastLogMessage(),
        ::testing::StartsWith("AU_ASSERT test failed:\nEXPRESSION: iter != _container.end()"));

    ASSERT_THROW(pScene->addInstance(
        "TeapotInstanceMissingGeom", geometry, {
            { Names::InstanceProperties::kMaterial, kMaterialPath },
            { Names::InstanceProperties::kMaterialLayers, vector<string>({kDecalMaterialPath0}) },
            { Names::InstanceProperties::kGeometryLayers, vector<string>({kInvalidPath}) },
             { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) }
        }), TestHelpers::AuroraLoggerException);
    ASSERT_THAT(lastLogMessage(),
        ::testing::StartsWith("AU_ASSERT test failed:\nEXPRESSION: iter != _container.end()"));
}

// TODO: Re-enable test when layers are working.
TEST_P(RendererTest, TestRendererInvalidGeometryLayers)
{
    // No layers on HGI backed.
    if(!isDirectX())
        return;

    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    const std::string txtNormalName = dataPath() + "/Textures/fishscale_normal.png";
    Path normalImagePath =  loadImage(txtNormalName, false);

    Path kMaterialPath = "BaseMaterial";
    pScene->setMaterialProperties(kMaterialPath, {{"normal_image", normalImagePath}, { "base_color", vec3(1, 0, 0) } });


    // Load opacity image.
    const std::string txtName = dataPath() +
        "/Textures/Triangle.png";
    const Path opacityImagePath = loadImage(txtName);;

    // Load diffuse image.
    const std::string mandrillTxPath = dataPath() + "/Textures/Mandrill.png";
    const Path mandrillImagePath = loadImage(mandrillTxPath);;
  
    // Create a material.
    const Path kDecalMaterialPath0 = "kDecalMaterial0";
    pScene->setMaterialProperties(kDecalMaterialPath0, { { "opacity_image", opacityImagePath }, { "base_color_image", mandrillImagePath }, {"diffuse_roughness",0.9f} });

    ;
    vector<glm::vec2> xformedUVs(TestHelpers::TeapotModel::verticesCount());
    for (uint32_t i = 0; i < xformedUVs.size(); i++) {
        xformedUVs[i].x = *(TestHelpers::TeapotModel::uvs()+(i*2+0));
        xformedUVs[i].y = *(TestHelpers::TeapotModel::uvs()+(i*2+1));
        xformedUVs[i]*=0.25;
        xformedUVs[i]+=0.5;
    }

    const Path kDecalUVGeomPath0 = "DecalUVGeomPath0";
    GeometryDescriptor geomDesc;
    geomDesc.type                                                       = PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
    geomDesc.vertexDesc.count = TestHelpers::TeapotModel::verticesCount();
    geomDesc.getAttributeData = [&xformedUVs](AttributeDataMap& buffers, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == TestHelpers::TeapotModel::verticesCount(), "Partial update not supported");
        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == 0, "Partial update not supported");
        buffers[Names::VertexAttributes::kTexCoord0].address = &xformedUVs[0];
        buffers[Names::VertexAttributes::kTexCoord0].size =
            TestHelpers::TeapotModel::verticesCount() * sizeof(vec2);
        buffers[Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

        return true;
    };
    pScene->setGeometryDescriptor(kDecalUVGeomPath0, geomDesc);

    const Path kInvalidGeomPath = "InvalidGeom";
    GeometryDescriptor invalidGeomDesc;
    vec2 invalidUVs[] = {{0,1},{1,1},{0,0}};

    invalidGeomDesc.type                                                       = PrimitiveType::Triangles;
    invalidGeomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
    invalidGeomDesc.vertexDesc.count = sizeof(invalidUVs)/sizeof(vec2);
    invalidGeomDesc.getAttributeData = [&invalidUVs](AttributeDataMap& buffers, size_t firstVertex,
                                    size_t vertexCount, size_t firstIndex, size_t indexCount) {
        AU_ASSERT(firstVertex == 0, "Partial update not supported");
        AU_ASSERT(vertexCount == sizeof(invalidUVs)/sizeof(vec2), "Partial update not supported");
        AU_ASSERT(firstIndex == 0, "Partial update not supported");
        AU_ASSERT(indexCount == 0, "Partial update not supported");
        buffers[Names::VertexAttributes::kTexCoord0].address = &invalidUVs[0];
        buffers[Names::VertexAttributes::kTexCoord0].size =
            sizeof(invalidUVs);
        buffers[Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);

        return true;
    };
    pScene->setGeometryDescriptor(kInvalidGeomPath, invalidGeomDesc);

    Path geometry      = createTeapotGeometry(*pScene);

    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, {
            { Names::InstanceProperties::kMaterial, kMaterialPath },
            { Names::InstanceProperties::kMaterialLayers, vector<string>({kDecalMaterialPath0}) },
            { Names::InstanceProperties::kGeometryLayers, vector<string>({kInvalidGeomPath}) },
             { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) }
        }));

    ASSERT_THROW(pRenderer->render(), TestHelpers::AuroraLoggerException);
    ASSERT_THAT(lastLogMessage(),
        ::testing::StartsWith("Rendering has failed"));
}

// Test instance with layer materials
// TODO: Re-enable test when layers are working.
TEST_P(RendererTest, TestRendererMultipleMaterialLayers)
{
    if (!isDirectX())
        return;

    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    const std::string txtNormalName = dataPath() + "/Textures/fishscale_normal.png";    
    const Path normalImagePath = loadImage(txtNormalName);

    Path kMaterialPath = "BaseMaterial";
    pScene->setMaterialProperties(kMaterialPath, { { "base_color", vec3(1, 0, 0) } });


    // Load opacity image.
    const std::string txtName = dataPath() +
        "/Textures/Triangle.png";
    const Path opacityImagePath = loadImage(txtName);

    // Load metallic image.
    const std::string metalTxtPath = dataPath() + "/Textures/fishscale_roughness.png";
    const Path metalImagePath = loadImage(metalTxtPath);;    

    // Load diffuse image
    const std::string mandrillTxPath = dataPath() + "/Textures/Mandrill.png";
    const Path mandrillImagePath =  loadImage(mandrillTxPath);

    // Create two decal materials.
    vector<Path> materialLayers = {"DecalMaterial0","DecalMaterial1"} ;
    pScene->setMaterialProperties(materialLayers[0], { {"normal_image", normalImagePath},{ "opacity_image", opacityImagePath }, { "base_color_image", metalImagePath }, {"diffuse_roughness",0.2f} });
    pScene->setMaterialProperties(materialLayers[1], {  { "opacity_image", opacityImagePath }, { "base_color_image", mandrillImagePath }, {"diffuse_roughness",0.6f}, {"metalness",0.9f} });

    int numLayers = 2;
    vector<string> geometryLayers;

    float scales[] = {1,2};
    glm::vec2 origins[] = {glm::vec2(0.75f,1.0f),glm::vec2(-0.25f,1.2f)};

    vector<vector<glm::vec2>> xformedUVs(numLayers);

    for (int layer = 0; layer < numLayers; layer++) {
        auto &uvs = xformedUVs[layer];
        uvs.resize(TestHelpers::TeapotModel::verticesCount());

        for (uint32_t i = 0; i < uvs.size(); i++) {
            uvs[i].x = *(TestHelpers::TeapotModel::vertices()+(i*3+0));
            uvs[i].y = *(TestHelpers::TeapotModel::vertices()+(i*3+1));
            uvs[i]=origins[layer]-uvs[i];
            uvs[i]*=scales[layer];
        }

        const Path kDecalUVGeomPath = "DecalUVGeomPath"+to_string(layer);
        GeometryDescriptor geomDesc;
        geomDesc.type                                                       = PrimitiveType::Triangles;
        geomDesc.vertexDesc.attributes[Names::VertexAttributes::kTexCoord0] = AttributeFormat::Float2;
        geomDesc.vertexDesc.count = TestHelpers::TeapotModel::verticesCount();
        geomDesc.getAttributeData = [&xformedUVs, layer](AttributeDataMap& buffers, size_t firstVertex,
                                        size_t vertexCount, size_t firstIndex, size_t indexCount) {
            AU_ASSERT(firstVertex == 0, "Partial update not supported");
            AU_ASSERT(vertexCount == TestHelpers::TeapotModel::verticesCount(), "Partial update not supported");
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

    for (int layer = 0; layer < numLayers; layer++) {
        const Path kDecalUVGeomPath = "DecalUVGeomPath"+to_string(layer);
        geometryLayers.push_back(kDecalUVGeomPath);
    }


    // Create a teapot instance with a default material.
    Path geometry      = createTeapotGeometry(*pScene);
    Path planeGeometry = createPlaneGeometry(*pScene);
    EXPECT_TRUE(pScene->addInstance(
        "TeapotInstance", geometry, {
            { Names::InstanceProperties::kMaterial, kMaterialPath },
            { Names::InstanceProperties::kMaterialLayers,  materialLayers},
            { Names::InstanceProperties::kGeometryLayers, geometryLayers },
             { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, -0.5, -0.8)) }
        }));
    EXPECT_TRUE(pScene->addInstance(
        "Plane", planeGeometry, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));

    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Ensure renderer works with different geographic locales.
// NOTE: This is necessary due to internal string manipulations inside the renderer.
TEST_P(RendererTest, TestLocales)
{

    // Relies on MaterialX which doesn't work on HGI.
    if (!isDirectX())
        return;

    // Selection of locales.
    // NOTE: These locales represent a wide set of different behaviors, and are not redundant with
    // each other.
    // clang-format off
    vector<string> locales = {
        "zh-cn", // China
        "de_DE", // Germany
        "am_ET", // Ethiopia, Amharic
        "am",    // Amharic
        "hi_IN", // India, Hindi
        "sq"     // Albanian
    };
    // clang-format off

    // Run renderer for each locale.
    for (size_t i=0; i<locales.size(); i++)
    {
        // Get the current locale string.
        string locStr = locales[i];
        AU_INFO("Setting locale to :%s",locStr.c_str());

        try
        {
            // Set the locale.
            std::locale thisLocale(locStr);
            std::locale::global(thisLocale);
        }
        catch (const std::runtime_error&)
        {
            AU_WARN("Unsupported locale:%s", locStr.c_str());
            continue;
        }

        // Create the default scene (also creates renderer)
        auto pScene    = createDefaultScene();
        auto pRenderer = defaultRenderer();

        // If pRenderer is null this renderer type not supported, skip rest of the test.
        if (!pRenderer)
            return;

        // Create teapot geom.
        Path geometry = createTeapotGeometry(*pScene);

        // Try loading a MtlX file dumped from HdAurora.
        Path oxideMaterial("LocaleHdAuroraMaterial");
        pScene->setMaterialType(oxideMaterial, Names::MaterialTypes::kMaterialXPath, dataPath() + "/Materials/TestMaterial.mtlx");

        // Add to scene.
        Path instance("LocalHdAuroraInstance");
        Properties instProps;
        instProps[Names::InstanceProperties::kMaterial] = oxideMaterial;
        EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

        // Render scene.
        pRenderer->render();

        // Ensure no warnings or errors.
        ASSERT_EQ(errorAndWarningCount(), 0);
    }
}

// Test orthographic projection.
TEST_P(RendererTest, TestRendererOrthographicProjection)
{
    // Create the default scene and renderer.
    Aurora::IScenePtr pScene       = createDefaultScene();
    Aurora::IRendererPtr pRenderer = defaultRenderer();

    // If the renderer is null, then this renderer type not supported, so skip the rest of the test.
    if (!pRenderer)
    {
        return;
    }

    // Set a camera with orthographic projection on the renderer.
    const float kSize = 1.5f;
    glm::mat4 projMtx = glm::orthoRH(-kSize, kSize, -kSize, kSize, 0.1f, 1000.0f);
    glm::mat4 viewMtx = glm::lookAt(
        glm::vec3(5.0f, 5.0f, -5.0f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    pRenderer->setCamera(glm::value_ptr(viewMtx), glm::value_ptr(projMtx));

    // Create a teapot instance with the default material.
    Path geometry = createTeapotGeometry(*pScene);

    // Add the instance to the scene.
    Path instance("OrthographicProjectionInstance");
    Properties instProps;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Ensure result is correct by comparing to the baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Test alpha output.
TEST_P(RendererTest, TestRendererAlpha)
{
    // Create the default scene and renderer.
    Aurora::IScenePtr pScene       = createDefaultScene();
    Aurora::IRendererPtr pRenderer = defaultRenderer();

    // If the renderer is null, then this renderer type not supported, so skip the rest of the test.
    if (!pRenderer)
    {
        return;
    }

    // Alpha output not implemented on HGI.
    if (!isDirectX())
        return;

    // Enable alpha output on the renderer.
    pRenderer->options().setBoolean("alphaEnabled", true);

    // Set a camera with orthographic projection on the renderer.
    const float kSize = 1.5f;
    glm::mat4 projMtx = glm::orthoRH(-kSize, kSize, -kSize, kSize, 0.1f, 1000.0f);
    glm::mat4 viewMtx = glm::lookAt(
        glm::vec3(5.0f, 5.0f, -5.0f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    pRenderer->setCamera(glm::value_ptr(viewMtx), glm::value_ptr(projMtx));

    // Create a teapot instance with the default material.
    Path geometry = createTeapotGeometry(*pScene);

    // Add the instance to the scene.
    Path instance("OrthographicProjectionInstance");
    Properties instProps;
    EXPECT_TRUE(pScene->addInstance(instance, geometry, instProps));

    // Ensure result is correct by comparing to the baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}

// Ensure lighting behind an opaque surface has no effect.
TEST_P(RendererTest, TestRendererBackFaceLighting)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    // Rotate directional light facing to back-face of quad.
    defaultDistantLight()->values().setFloat(
        Aurora::Names::LightProperties::kIntensity, 2.0f);
    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kDirection, value_ptr(glm::vec3(0, 0, -1)));
    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kColor, value_ptr(glm::vec3(1, 1, 1)));

    // Create a material (setting material type to default triggers creation.)
    const Path kMaterialPath = "DefaultMaterial";
    pScene->setMaterialType(kMaterialPath);

    // Create teapot and plane instance.
    Path teapotPath = createTeapotGeometry(*pScene);
    Path planePath = createPlaneGeometry(*pScene);

    // Create teapot behind opaque plane, with normal facing away from cameras.
    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath, { { Names::InstanceProperties::kMaterial, kMaterialPath } }));
    EXPECT_TRUE(pScene->addInstance(nextPath(), planePath, { { Names::InstanceProperties::kMaterial, kMaterialPath }, { Names::InstanceProperties::kTransform, glm::translate(glm::vec3(0, 0, -0.75)) * glm::rotate(static_cast<float>(M_PI), glm::vec3(0, 1, 0))}, }));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}


// Normal map image test.
TEST_P(RendererTest, TestDebugNormals)
{
    // Create the default scene (also creates renderer)
    auto pScene    = createDefaultScene();
    auto pRenderer = defaultRenderer();

    if(!pRenderer)
        return;

    Aurora::IValues& options = pRenderer->options();

    setDefaultRendererSampleCount(1);
    
    //Set debug mode to normals (kDebugModeNormal==3)
    options.setInt("debugMode", 3);

    // If pRenderer is null this renderer type not supported, skip rest of the test.
    if (!pRenderer)
        return;

    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kDirection, value_ptr(glm::vec3(0.0f, -0.25f, +1.0f)));
    defaultDistantLight()->values().setFloat3(
        Aurora::Names::LightProperties::kColor, value_ptr(glm::vec3(1, 1, 1)));

    // Load pixels for test image file.
    const std::string txtName = dataPath() + "/Textures/Verde_Guatemala_Slatted_Marble_normal.png";
    const Path imagePath          = loadImage(txtName, false);

    // Create geometry.
    Path planePath  = createPlaneGeometry(*pScene, vec2(0.5f,0.5f));
    Path teapotPath = createTeapotGeometry(*pScene);

    // Create matrix and material for first instance (which is linearized).
    const Path kMaterialPath = "NormalMaterial";
    pScene->setMaterialProperties(kMaterialPath, { { "normal_image", imagePath },{ "normal_image_scale", vec2(5.f, 5.f)} });
    const Path kBasicMaterialPath = "BasicMaterial";
    pScene->setMaterialProperties(kBasicMaterialPath, { { "base_color", vec3(0,1,0) } });
    
    mat4 scaleMtx = scale(vec3(2, 2, 2));

    // Create geometry with the material.
    Properties instProps;
    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath;
    instProps[Names::InstanceProperties::kTransform] = scaleMtx;
    EXPECT_TRUE(pScene->addInstance(nextPath(), planePath, instProps));

    instProps[Names::InstanceProperties::kMaterial]  = kMaterialPath;
    instProps[Names::InstanceProperties::kTransform] = mat4();
    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath, instProps));

    instProps[Names::InstanceProperties::kMaterial]  = kBasicMaterialPath;
    instProps[Names::InstanceProperties::kTransform] = translate(vec3(0,-1.5,0));
    EXPECT_TRUE(pScene->addInstance(nextPath(), teapotPath, instProps));

    // Render the scene and check baseline image.
    ASSERT_BASELINE_IMAGE_PASSES(currentTestName());
}


INSTANTIATE_TEST_SUITE_P(RendererTests, RendererTest, TEST_SUITE_RENDERER_TYPES());

} // namespace

#endif
