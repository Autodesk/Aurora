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
#pragma once

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

#include "BaselineImageHelpers.h"

using namespace std;
#include <Aurora/Aurora.h>

// GLM used by all renderers tests
#define GLM_FORCE_CTOR_INIT
#pragma warning(push)
#pragma warning(disable : 4127) // nameless struct/union
#pragma warning(disable : 4201) // conditional expression is not constant
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/transform.hpp"
#pragma warning(pop)

/// Convenience helper functions for internal use in renderers unit tests
namespace TestHelpers
{

// Geometry for teapot test model created from OBJ
class TeapotModel
{
public:
    // Gets a pointer to vertices for teapot test model
    static const float* vertices();

    // Gets a pointer to normals for teapot test model
    static const float* normals();

    // Gets a pointer to normals for teapot test model
    static const float* uvs();

    // Gets a pointer to tangents for teapot test model
    static const float* tangents();

    // Gets the number of floats in teapot vertices
    static uint32_t verticesCount();

    // Gets a pointer to indices for teapot test model
    static const unsigned int* indices();

    // Gets the number of indices for teapot test model
    static uint32_t indicesCount();
};

/// \brief Shared test fixture for all Renderers tests
class FixtureBase : public testing::TestWithParam<string>
{
public:
    FixtureBase();
    ~FixtureBase() {}

    /// Get the data path used for test assets.
    const string& dataPath() { return _dataPath; }

    Aurora::Path loadImage(const string& filename, bool linearize = true);

    /// \brief Creates a default renderer and render buffer for renderer tests to use if needed for
    /// conciseness (and associated render buffer.)
    /// \param width Width of the default render buffer.
    /// \param height Height of the default render buffer.
    /// \return Returns the default renderer (or null if the current renderer type is not
    /// supported.)
    Aurora::IRendererPtr createDefaultRenderer(int width = 128, int height = 128);

    /// \brief Creates the geometry for a simple test teapot geometry with default renderer.
    /// \return Path for new geometry for teapot.
    Aurora::Path createTeapotGeometry(Aurora::IScene& scene);

    /// \brief Creates the geometry for a simple test teapot geometry with default renderer.
    /// \return Path for new geometry.
    Aurora::Path createPlaneGeometry(Aurora::IScene& scene, glm::vec2 uvScale = glm::vec2(1, 1),
        glm::vec2 uvOffset = glm::vec2(0, 0));

    /// \brief Creates a default scene for renderer tests to use if needed for conciseness (and the
    /// default renderer if it hasn't been created already.)
    /// \return the default scene (or null if the current renderer type is not supported.)
    Aurora::IScenePtr createDefaultScene();

    /// \brief Renders the default scene and runs a baseline image comparison test on the resulting
    /// pixels
    /// \return the baseline image comparison result calculated from the rendered pixels
    TestHelpers::BaselineImageComparison::Result renderAndCheckBaselineImage(
        const string& name, const string& folder = "");

    /// \brief Gets the IRenderer::Backend for this instance of parameterized test
    Aurora::IRenderer::Backend rendererBackend() { return _typeLookup[GetParam()]; }

    /// \brief Gets the IRenderer::Backend for this instance of parameterized test
    bool backendSupported()
    {
        return _supportedBackends.find(GetParam()) != _supportedBackends.end();
    }

    /// \brief Are we testing DirectX backend ?
    bool isDirectX() { return rendererBackend() == Aurora::IRenderer::Backend::DirectX; }

    /// \brief Gets a human readable description to print to console.
    string rendererDescription() { return "Renderer type:" + GetParam(); }

    /// \brief Gets the default renderer. (null if createDefaultRenderer() not called)
    Aurora::IRendererPtr defaultRenderer() { return _pDefaultRenderer; }

    /// \brief Gets the default renderer's render buffer. (null if createDefaultRenderer() not
    /// called)
    Aurora::IRenderBufferPtr defaultRenderBuffer() { return _pDefaultRenderBuffer; }

    /// \brief Gets the default scene. (null if createDefaultScene not called)
    Aurora::IScenePtr defaultScene() { return _pDefaultScene; }

    Aurora::ILightPtr defaultDistantLight() { return _pDefaultDistantLight; }

    /// \brief Gets the default renderer's render buffer width.
    size_t defaultRendererWidth() { return _defaultRendererWidth; }

    /// \brief Gets the default renderer's render buffer height.
    size_t defaultRendererHeight() { return _defaultRendererHeight; }

    /// \brief Path used for baseline image tests to write baseline image files.
    const string& renderedBaselineImagePath() { return _renderedBaselineImagePath; }

    /// \brief Path used for baseline image tests to write output image files.
    const string& renderedOutputImagePath() { return _renderedOutputImagePath; }

    /// \brief Gets the name of current gtest for this fixture, combined with current renderer name.
    string currentTestName()
    {
        return string(testing::UnitTest::GetInstance()->current_test_info()->name()) + "." +
            GetParam();
    }

    /// \brief Set the sample count in baseline image renders.
    /// \note Total number sample count in the final image is product of sample count and number of
    /// iterations
    void setDefaultRendererSampleCount(uint32_t sampleCount)
    {
        _defaultRendererSampleCount = sampleCount;
    }

    /// Set the camera view matrix for the default renderer.
    void setDefaultRendererCamera(
        const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));

    /// Set perspective projection matrix for the default renderer.
    void setDefaultRendererPerspective(float fovDeg, float near = 0.1f, float far = 1000.0f);

    /// \brief Set the number of iterations in PT baseline image renders.
    /// \note Total number sample count in the final image is product of sample count and number of
    /// iterations
    void setDefaultRendererPathTracingIterations(uint32_t iter)
    {
        _defaultPathTracingIterations = iter;
    }

    /// \brief Sets the threshold values used in baseline image tests
    /// carried out by renderAndCheckBaselineImage.
    /// \param pixelFailPercent Threshold for percentage pixel
    /// difference allowed before failure.
    /// \param maxPercentFailingPixels Max percentage of failing
    /// pixels allowed before comparison fails.
    /// \param pixelWarningPercent Threshold for percentage pixel
    /// difference allowed before warning.
    /// \param maxPercentWarningPixels Max percentage of warning
    /// pixels allowed before comparison fails.
    void setBaselineImageThresholds(float pixelFailPercent, float maxPercentFailingPixels,
        float pixelWarnPercent, float maxPercentWarningPixels);

    /// \brief Resets the threshold values used in baseline image tests carried out by
    /// renderAndCheckBaselineImage to their default values.
    void resetBaselineImageThresholdsToDefaults()
    {
        setBaselineImageThresholds(0.2f, 0.05f, 0.05f, 0.25f);
    }

    /// \brief The renderer and scene will be destroyed by the fixture's destructor, but can also be
    /// explicitly destroyed by the test.
    void destroyRenderer()
    {
        _pDefaultRenderBuffer.reset();
        _pDefaultScene.reset();
        _pDefaultRenderer.reset();
    }

    const string lastLogMessage() const { return _lastLogMessage; }
    int errorAndWarningCount() const { return _errorAndWarningCount; }

    /// \brief Helper functions used to test setting values by type
    void testFloatValue(Aurora::IScene& scene, const Aurora::Path& material, const string name,
        bool exists, const string& message);
    void testFloat3Value(Aurora::IScene& scene, const Aurora::Path& material, const string& name,
        bool exists, const string& message);
    void testMatrixValue(Aurora::IScene& scene, const Aurora::Path& material, const string& name,
        bool exists, const string& message);
    void testBooleanValue(Aurora::IScene& scene, const Aurora::Path& material, const string& name,
        bool exists, const string& message);

    void testFloat3Option(
        Aurora::IRenderer& renderer, const string& name, bool exists, const string& message);
    void testFloatOption(
        Aurora::IRenderer& renderer, const string& name, bool exists, const string& message);
    void testBooleanOption(
        Aurora::IRenderer& renderer, const string& name, bool exists, const string& message);
    void testIntOption(
        Aurora::IRenderer& renderer, const string& name, bool exists, const string& message);

    /// Returns a unique (generated) path.
    Aurora::Path nextPath(string typeName = "")
    {
        return currentTestName() + (typeName.empty() ? "" : "/" + typeName) + "/" +
            to_string(_nextPath++);
    }

private:
    string _dataPath;

    // We map from string to IRenderer::Backend so types names have human-readable string type, not
    // int
    map<string, Aurora::IRenderer::Backend> _typeLookup;

    // Width used for default renderer's render buffer
    uint32_t _defaultRendererWidth = 128;

    // Height used for default renderer's render buffer
    uint32_t _defaultRendererHeight = 128;

    // Sample count used for default renderer's render buffer
    uint32_t _defaultRendererSampleCount = 4;

    // Number of iterations
    uint32_t _defaultPathTracingIterations = 32;

    // Thresholds used for baseline image comparison in renderAndCheckBaselineImage
    TestHelpers::BaselineImageComparison::Thresholds baselineImageThresholds;

    // Default renderer object
    Aurora::IRendererPtr _pDefaultRenderer;

    // Default distant light.
    Aurora::ILightPtr _pDefaultDistantLight;

    // Default renderer's render buffer object
    Aurora::IRenderBufferPtr _pDefaultRenderBuffer;

    // Default scene object
    Aurora::IScenePtr _pDefaultScene;

    // Path used for baseline image tests to write baseline image files
    string _renderedBaselineImagePath;

    // Path used for baseline image tests to write output image files
    string _renderedOutputImagePath;

    // Last log message received by Aurora::Foundation::Log logger.
    string _lastLogMessage;
    // Total number of log messages with level warning or greater received.
    int _errorAndWarningCount = 0;

    // Camera matrices.
    glm::mat4 _projMtx;
    glm::mat4 _viewMtx;

    // Next generated path.
    uint32_t _nextPath = 0;

    set<string> _supportedBackends = { AURORA_BACKENDS };
};

// Convenience macro to render with renderAndCheckBaselineImage and ensure baseline image result is
// "Passed"
#define ASSERT_BASELINE_IMAGE_PASSES(_name)                                                        \
    ASSERT_STREQ(renderAndCheckBaselineImage(_name).toString().c_str(), "Passed")                  \
        << rendererDescription()

// Convenience macro to render with renderAndCheckBaselineImage with a sub-folder and ensure
// baseline image result is "Passed"
#define ASSERT_BASELINE_IMAGE_PASSES_IN_FOLDER(_name, _folder)                                     \
    ASSERT_STREQ(renderAndCheckBaselineImage(_name, _folder).toString().c_str(), "Passed")         \
        << rendererDescription()

// The renderer type strings to pass to INSTANTIATE_TEST_SUITE_P
#define TEST_SUITE_RENDERER_TYPES() testing::Values("DirectX", "HGI")

} // namespace TestHelpers
