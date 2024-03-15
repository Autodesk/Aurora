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

#include "Camera.h"
#include "Loaders.h"
#include "PerformanceMonitor.h"
#include "SceneContents.h"

// Structure representing layer geometry and material.
struct Layer
{
    Aurora::GeometryDescriptor geomDesc;
    Aurora::Path geomPath;
    Aurora::Path mtlPath;
    vector<vec2> uvs;
};

// Vector of layers
using Layers = vector<Layer>;

class Plasma
{
public:
    /*** Lifetime Management ***/

#if defined(INTERACTIVE_PLASMA)
    explicit Plasma(HINSTANCE hInstance, unsigned int width = 1280, unsigned int height = 720);
#else
    explicit Plasma(unsigned int width = 1280, unsigned int height = 720);
#endif
    ~Plasma();

    /*** Functions **/
#if defined(INTERACTIVE_PLASMA)
    bool run();
#else
    bool run(int argc, char* argv[]);
#endif

private:
    /*** Private Types ***/

    using LoadFileFunction    = function<bool(const string&)>;
    using LoadFileFunctionMap = unordered_map<string, LoadFileFunction>;

    /*** Private Static Functions ***/
#if defined(INTERACTIVE_PLASMA)
    static LRESULT __stdcall wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif
    static void createSampleScene(Aurora::IScene* pRenderer, SceneContents& contentsOut);

    /*** Private Functions ***/

#if defined(INTERACTIVE_PLASMA)
    LRESULT processMessage(UINT message, WPARAM wParam, LPARAM lParam);
    HWND createWindow(const uvec2& dimensions);
#endif
    bool getFloat3Option(const string& name, glm::vec3& value) const;
#if defined(INTERACTIVE_PLASMA)
    void parseOptions();
#else
    void parseOptions(int argc, char* argv[]);
#endif
    bool initialize();
    void updateNewScene();
    void updateLighting();
    void updateGroundPlane();
    void updateSampleCount();
    void update();
#if defined(INTERACTIVE_PLASMA)
    void requestUpdate(bool shouldRestart = true);
    void toggleAnimation();
    void toggleFullScreen();
    void toggleVSync();
    void adjustUnit(int increment);
    void adjustExposure(float increment);
    void adjustMaxLuminanceExposure(float increment);
    void selectFile(
        const string& extension, const wchar_t* pFilters, const LoadFileFunction& loadFunc);
#endif
    bool loadEnvironmentImageFile(const string& filePath);
    bool loadSceneFile(const string& filePath);
    void saveImage(const wstring& filePath, const uvec2& dimensions);
    bool applyMaterialXFile(const string& mtlxPath);
    Aurora::Path loadMaterialXFile(const string& filePath);
    void addAssetPath(const string& path);
    void addAssetPathContainingFile(const string& filePath);

    void resetMaterials();
    bool addDecal(const string& decalMtlXPath);

#if defined(INTERACTIVE_PLASMA)
    /*** Private Event Handlers ***/

    void onFilesDropped(HDROP hDrop);
    void onKeyPressed(WPARAM keyCode);
    void onMouseMoved(int xPos, int yPos, WPARAM buttons);
    void onMouseWheel(int delta, WPARAM buttons);
    void onSizeChanged(UINT width, UINT height);
#endif
    /*** Private Variables ***/

#if defined(INTERACTIVE_PLASMA)
    HINSTANCE _hInstance                 = nullptr;
    HWND _hwnd                           = nullptr;
    WINDOWPLACEMENT _prevWindowPlacement = {};
#endif
    uvec2 _dimensions = uvec2(1280, 720);
    LoadFileFunctionMap _loadFileFunctions;
    unique_ptr<cxxopts::ParseResult> _pArguments;
    Camera _camera;
    Foundation::SampleCounter _sampleCounter;
    PerformanceMonitor _performanceMonitor;
    unsigned int _debugMode = 0;
    SceneContents _sceneContents;
    unsigned int _frameNumber = 0;
    bool _isAnimating         = false;
#if defined(INTERACTIVE_PLASMA)
    bool _isFullScreenEnabled = false;
    bool _isVSyncEnabled      = false;
    bool _isOrthoProjection   = false;
#endif
    bool _isDirectionalLightEnabled = true;
#if defined(INTERACTIVE_PLASMA)
    unsigned int _importanceSamplingMode = 2; // Importance sampling mode 2 == MIS
    bool _isReferenceBSDFEnabled         = false;
#endif
    vec3 _lightDirection     = normalize(vec3(1.0f, -0.5f, 0.0f));
    bool _isDenoisingEnabled = false;
#if defined(INTERACTIVE_PLASMA)
    bool _isDiffuseOnlyEnabled        = false;
    bool _isForceOpaqueShadowsEnabled = false;
    bool _isToneMappingEnabled        = false;
#endif
    bool _isGroundPlaneShadowEnabled     = false;
    bool _isGroundPlaneReflectionEnabled = false;
#if defined(INTERACTIVE_PLASMA)
    int _traceDepth             = 5;
    float _exposure             = 0.0f;
    float _maxLuminanceExposure = 0.0f;
#endif
    bool _shouldRestart   = true;
    vector<string> _units = { "millimeter", "centimeter", "inch", "foot", "yard" };
#if defined(INTERACTIVE_PLASMA)
    int _currentUnitIndex = 1;
#endif

    vec3 _lightStartDirection = vec3(1.0f, -0.5f, 0.0f);
    vec3 _lightColor          = vec3(1.0f, 1.0f, 1.0f);
    float _lightIntensity     = 2.0f;

    Foundation::CPUTimer _animationTimer;
    string _materialXFilePath;
    string _decalMaterialXFilePath;
    vector<Layers> _instanceLayers;

    Aurora::Path _environmentPath;

    Aurora::IRenderer::Backend _rendererType = Aurora::IRenderer::Backend::Default;
    Aurora::IRendererPtr _pRenderer;
    Aurora::IGroundPlanePtr _pGroundPlane;
    Aurora::ILightPtr _pDistantLight;
    Aurora::IScenePtr _pScene;
    Aurora::IWindowPtr _pWindow;
    vector<string> _assetPaths;
};
