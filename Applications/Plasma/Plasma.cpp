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
#include "pch.h"

#include "Plasma.h"

#include "Loaders.h"

// A global pointer to the application object, for the static WndProc function.
Plasma* gpApp = nullptr;
#if defined(INTERACTIVE_PLASMA)
static const char* kAppName = "Plasma";
#endif
// The maximum number of samples to render when converging.
constexpr uint32_t kMaxSamples       = 1000;
constexpr uint32_t kDenoisingSamples = 50;

#if defined(INTERACTIVE_PLASMA)
// Application constructor.
Plasma::Plasma(HINSTANCE hInstance, unsigned int width, unsigned int height)
{
    // Initialize member variables.
    _hInstance    = hInstance;
    _dimensions.x = width > 0 ? width : _dimensions.x;
    _dimensions.y = height > 0 ? height : _dimensions.y;

    // Initialize the sample counter.
    _sampleCounter.setMaxSamples(kMaxSamples);
    _sampleCounter.reset();

    // Initialize the file loading functions, based on file extension.
    _loadFileFunctions[".hdr"]  = bind(&Plasma::loadEnvironmentImageFile, this, placeholders::_1);
    _loadFileFunctions[".mtlx"] = bind(&Plasma::applyMaterialXFile, this, placeholders::_1);
    _loadFileFunctions[".obj"]  = bind(&Plasma::loadSceneFile, this, placeholders::_1);

    // Set a status update function on the performance monitor.
    _performanceMonitor.setStatusFunction([this](const string& message) {
        // Start building a report string with scene information and performance stats.
        stringstream report;
        report.imbue(locale("")); // thousands separator for integers
        report << kAppName;
        if (_sceneContents.instances.size() > 0)
        {
            report << "  |  " << _sceneContents.vertexCount << " vertices / "
                   << _sceneContents.triangleCount << " triangles / "
                   << _sceneContents.instances.size() << " instance(s)";
        }

        // Add the status report from the performance monitor.
        report << message;

        // Set the complete report as the window title.
        ::SetWindowTextW(_hwnd, Foundation::s2w(report.str()).c_str());
    });
}
#else  //! INTERACTIVE_PLASMA
// Application constructor.
Plasma::Plasma(unsigned int width, unsigned int height)
{
    // Initialize member variables.
    _dimensions.x = width > 0 ? width : _dimensions.x;
    _dimensions.y = height > 0 ? height : _dimensions.y;

    // Initialize the sample counter.
    _sampleCounter.setMaxSamples(kMaxSamples);
    _sampleCounter.reset();

    // Initialize the file loading functions, based on file extension.
    _loadFileFunctions[".hdr"]  = bind(&Plasma::loadEnvironmentImageFile, this, placeholders::_1);
    _loadFileFunctions[".mtlx"] = bind(&Plasma::applyMaterialXFile, this, placeholders::_1);
    _loadFileFunctions[".obj"]  = bind(&Plasma::loadSceneFile, this, placeholders::_1);
}
#endif // INTERACTIVE_PLASMA

// Application destructor.
Plasma::~Plasma()
{
    // These hold a shared pointer to the scene object, need to clear them before renderer dtor
    // triggered.
    _sceneContents.reset();
}

#if defined(INTERACTIVE_PLASMA)
// Runs the application message loop until the user quits the application.
bool Plasma::run()
{
    // Parse the command line arguments as options.
    // NOTE: This will exit the application if the help argument is supplied.
    parseOptions();

    // Initialize the application, and return immediately if that does not succeed.
    if (!initialize())
    {
        ::errorMessage("Failed to initialize Plasma.");

        return false;
    }

    // Show the window.
    ::ShowWindow(_hwnd, SW_SHOWNORMAL);

    // Process system messages until a WM_QUIT message is received.
    MSG msg = {};
    while (::GetMessage(&msg, nullptr, 0, 0) != 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return true;
}

// The static window callback function.
LRESULT __stdcall Plasma::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Process the message.
    LRESULT result = gpApp->processMessage(message, wParam, lParam);

    // Return the result if valid, otherwise use default message processing.
    return result == -1 ? DefWindowProc(hWnd, message, wParam, lParam) : result;
}
#else
// Runs the application as one-pass command line program
bool Plasma::run(int argc, char* argv[])
{
    // Parse the command line arguments as options.
    // NOTE: This will exit the application if the help argument is supplied.
    parseOptions(argc, argv);

    // Initialize the application, and return immediately if that does not succeed.
    if (!initialize()) // TODO single image rendering starts and ends inside initialize().
    {
        ::errorMessage("Failed to initialize Plasma.");

        return false;
    }

    return true;
}
#endif

// Creates a sample scene.
Aurora::IScenePtr Plasma::createSampleScene(Aurora::IRenderer* pRenderer, SceneContents& contents)
{
    // Clear the result.
    contents.reset();

    // Create an Aurora scene.
    Aurora::IScenePtr pScene = pRenderer->createScene();
    if (!pScene)
    {
        return nullptr;
    }

    // Create sample geometry, a single triangle.
    const Aurora::Path kGeomPath = "PlasmaDefaultSceneGeometry";

    contents.geometry[kGeomPath]    = SceneGeometryData();
    SceneGeometryData& geometryData = contents.geometry[kGeomPath];

    // clang-format off
    geometryData.positions = {
         0.0f,   0.0f, 0.0f,
         0.25f, -0.5f, 0.0f,
        -0.25f, -0.5f, 0.0f,
    };
    geometryData.normals = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    // clang-format on

    Aurora::GeometryDescriptor& geomDesc = geometryData.descriptor;
    geomDesc.type                        = Aurora::PrimitiveType::Triangles;
    geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kPosition] =
        Aurora::AttributeFormat::Float3;
    geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kNormal] =
        Aurora::AttributeFormat::Float3;
    geomDesc.vertexDesc.count = 3;
    geomDesc.indexCount       = 0;
    geomDesc.getAttributeData = [kGeomPath, &contents](Aurora::AttributeDataMap& buffers,
                                    size_t /* firstVertex*/, size_t /* vertexCount*/,
                                    size_t /* firstIndex*/, size_t /* indexCount*/) {
        SceneGeometryData& geometryData = contents.geometry[kGeomPath];

        buffers[Aurora::Names::VertexAttributes::kPosition].address = geometryData.positions.data();
        buffers[Aurora::Names::VertexAttributes::kPosition].size =
            geometryData.positions.size() * sizeof(float);
        buffers[Aurora::Names::VertexAttributes::kPosition].stride = sizeof(vec3);
        buffers[Aurora::Names::VertexAttributes::kNormal].address  = geometryData.normals.data();
        buffers[Aurora::Names::VertexAttributes::kNormal].size =
            geometryData.normals.size() * sizeof(float);
        buffers[Aurora::Names::VertexAttributes::kNormal].stride = sizeof(vec3);

        return true;
    };

    pScene->setGeometryDescriptor(kGeomPath, geomDesc);

    // Create several materials with varying base colors.
    const int kNumLevels    = 5;
    const int kNumChannels  = 4;
    const int kNumMaterials = kNumLevels * kNumChannels;
    vector<Aurora::Path> lstMaterials;
    int mtlIdx = 0;
    lstMaterials.reserve(kNumMaterials);
    for (int channel = 0; channel < kNumChannels; channel++)
    {
        for (int level = 1; level <= kNumLevels; level++)
        {
            Aurora::Path mtlPath = "PlasmaDefaultSceneMaterial:" + to_string(mtlIdx++);

            vec3 color;
            float channel_value = static_cast<float>(level) / kNumLevels;
            if (channel == 3)
            {
                color = vec3(channel_value); // grayscale
            }
            else
            {
                color[channel] = channel_value; // single channel
            }
            pScene->setMaterialProperties(mtlPath, { { "base_color", ::sRGBToLinear(color) } });
            lstMaterials.push_back(mtlPath);
        }
    }

    // Add several transformed instances of the sample geometry to the scene.
    constexpr float kDepth         = 10.0f;
    constexpr size_t kNumInstances = 200;
    constexpr float kFullAngle     = 8.0f * radians(360.0f);
    constexpr vec3 kRotateAxis     = vec3(0.0f, 0.0f, 1.0f);
    constexpr vec3 kOffset         = vec3(0.0f, -0.5f, 0.0);
    constexpr float kAngleOffset   = kFullAngle / kNumInstances;
    constexpr float kDepthOffset   = -kDepth / (kNumInstances - 1);

    // Retain a list of transforms for use in the callback function below.
    Aurora::InstanceDefinitions instanceData;
    instanceData.resize(kNumInstances);
    contents.instances.clear();
    for (size_t i = 0; i < kNumInstances; ++i)
    {
        instanceData[i] = { "DefaultSceneInstance" + to_string(i),
            { { Aurora::Names::InstanceProperties::kMaterial, lstMaterials[i % kNumMaterials] },
                { Aurora::Names::InstanceProperties::kTransform,
                    translate(rotate(mat4(), i * kAngleOffset, kRotateAxis),
                        kOffset + vec3(0.0f, 0.0f, i * kDepthOffset)) } } };

        contents.instances.push_back({ instanceData[i], kGeomPath });
    }

    // Add all the instances with material and transform variations.
    auto instancePaths = pScene->addInstances(kGeomPath, instanceData);

    // Specify a bounding box for the scene.
    vec3 min(-1.0f, -1.0f, -kDepth - 0.01f);
    vec3 max(1.0f, 1.0f, 0.01f);
    contents.bounds = Foundation::BoundingBox(min, max);

    return pScene;
}

bool Plasma::getFloat3Option(const string& name, glm::vec3& value) const
{
    // Get a reference to the parsed options result.
    cxxopts::ParseResult& arguments = *_pArguments;

    // Return false if no argument was supplied for the option.
    if (arguments.count(name) == 0)
    {
        return false;
    }

    // Get the argument for the specified option as a float vector. Return false if it doesn't
    // have three values.
    vector<float> floatVector = arguments[name].as<vector<float>>();
    if (floatVector.size() != 3)
    {
        return false;
    }

    // Convert the float vector to vec3.
    value = glm::make_vec3(floatVector.data());

    return true;
}

#if defined(INTERACTIVE_PLASMA)
void Plasma::parseOptions()
{
    // Command line arguments.
    int numArgs = 0;

    // Parse the command line string into argument pointers, and convert the arguments to narrow
    // strings so they can be parsed by cxxopts.
    LPWSTR* pArgs = ::CommandLineToArgvW(::GetCommandLine(), &numArgs);

    // Pre-allocate vectors for the arguments.
    vector<string> argsNarrow(numArgs);
    vector<char*> argsNarrowPtr(numArgs);

    for (int i = 0; i < numArgs; i++)
    {
        // Convert the argument to a UTF-8 string, and collect a non-const char pointer for it.
        // NOTE: The first array must be maintained for the duration, as the second array has
        // pointers to it.
        argsNarrow[i]    = Foundation::w2s(pArgs[i]);
        argsNarrowPtr[i] = const_cast<char*>(argsNarrow[i].c_str());
    }

    // Free the command line argument pointers.
    ::LocalFree(pArgs);
    pArgs = nullptr;

    // Get char pointer-to-pointer to pass to cxxopts.
    char** pArgv = argsNarrowPtr.data();
#else
void Plasma::parseOptions(int argc, char* argv[])
{
    // Command line arguments.
    int numArgs  = argc;
    char** pArgv = argv;

#endif
    // Initialize cxxopts options with application name.
    cxxopts::Options options("Plasma", "Plasma: Aurora example application.");

    // Set the help string to display for positional arguments.
    options.positional_help("<scene>");
    options.show_positional_help();

    // Initialize the cxxopts command line parser with the available options.
    // NOTE: The first option is the (optional) positional argument.
    // clang-format off
    options.add_options()
        ("scene", "Scene file path to load (Wavefront OBJ format)",cxxopts::value<string>())
        ("reference", "Use reference BSDF", cxxopts::value<bool>())
        ("denoise", "Enable denoising", cxxopts::value<bool>())
        ("renderer", "Renderer type ('dx for DirectX, hgi for HGI.)", cxxopts::value<string>())
        ("e,eye", "Camera eye position as comma-separated 3D vector (e.g. 1,2,3)", cxxopts::value<vector<float>>())
        ("t,target", "Camera target position as comma-separated 3D vector (e.g. 1,2,3)", cxxopts::value<vector<float>>())
        ("u,up", "Camera up vector as comma-separated 3D vector (e.g. 1,2,3)", cxxopts::value<vector<float>>())
        ("lightDir", "Directional light initial direction as comma-separated 3D vector (e.g. 1,2,3)", cxxopts::value<vector<float>>())
        ("lightColor", "Directional light color as comma-separated 3D vector (e.g. 1,2,3)", cxxopts::value<vector<float>>())
        ("lightIntensity", "Directional light intensity", cxxopts::value<float>())
        ("output", "Output image file (if set will render once and exit)", cxxopts::value<string>())
        ("dims", "Window dimensions", cxxopts::value<vector<int>>())
        ("fov", "Camera field of view in degrees.", cxxopts::value<float>())
        ("env", "Environment map path to load (lat-long format .HDR file)", cxxopts::value<string>())
        ("mtlx", "MaterialX document path to apply.",cxxopts::value<string>())
        ("h,help", "Print help");
    // clang-format on

    // Parse positional arguments, and then the remaining arguments. This will decrement
    // numArgs. NOTE: The stored ParseResult is assigned to a unique_ptr because the class does
    // not have a default constructor or assignment operator.
    options.parse_positional({ "scene" });
    _pArguments = make_unique<cxxopts::ParseResult>(options.parse(numArgs, pArgv));

    // If the help argument is supplied, display the print the usage message immediately exit.
    if (_pArguments->count("help"))
    {
        stringstream message;
        message << options.help();
#if defined(INTERACTIVE_PLASMA)
        wstring messageWide = Foundation::s2w(message.str());
        ::MessageBox(nullptr, messageWide.c_str(), L"Command line options", MB_OK);
#else
        cout << message.str() << endl;
#endif

        exit(0);
    }
}

// Initializes the application.
bool Plasma::initialize()
{
    cxxopts::ParseResult& arguments = *_pArguments;

    // Set the window dimensions based on dims argument.
    if (arguments.count("dims"))
    {
        vector<int> dims = arguments["dims"].as<vector<int>>();
        if (dims.size() == 2)
        {
            _dimensions.x = dims[0];
            _dimensions.y = dims[1];
        }
    }

#if defined(INTERACTIVE_PLASMA)
    // Create the application window.
    _hwnd = createWindow(_dimensions);
    ::setMessageWindow(_hwnd);
#endif

    // Parse the backend type from the argument for the "renderer" parameter: "dx" or "hgi".
    if (_pArguments->count("renderer"))
    {
        string renderArg = arguments["renderer"].as<string>();
        if (renderArg.compare("dx") == 0)
        {
            _rendererType = Aurora::IRenderer::Backend::DirectX;
        }
        else if (renderArg.compare("hgi") == 0)
        {
            _rendererType = Aurora::IRenderer::Backend::HGI;
        }
        else
        {
            ::errorMessage("Unknown renderer argument: " + renderArg);
        }
    }

    // Create an Aurora renderer.
    _pRenderer = Aurora::createRenderer(_rendererType);

    // Ensure images not flipped.
    _pRenderer->options().setBoolean("isFlipImageYEnabled", false);

    // Return false (failed initialization) if a renderer could not be created.
    if (!_pRenderer)
    {
        return false;
    }

    // Setup asset search paths. Including the path to the ProteinX test folder (if running within
    // the Github repo).
    // TODO: A more reliable solution would be to set based on the loaded materialX file path.
    _assetPaths = { "",
        Foundation::getModulePath() +
            "../../../Renderers/Tests/Data/Materials/mtlx_ProteinSubset/" };

    // Setup the resource loading function to use asset search paths.
    auto loadResourceFunc = [this](const string& uri, vector<unsigned char>* pBufferOut,
                                string* pFileNameOut) {
        // Iterate through all search paths.
        for (size_t i = 0; i < _assetPaths.size(); i++)
        {
            // Prefix the current search path to URI.
            string currPath = _assetPaths[i] + uri;

            // Open file.
            ifstream is(currPath, ifstream::binary);

            // If file could not be opened, continue.
            if (!is)
                continue;

            // Read contents to buffer.
            is.seekg(0, is.end);
            size_t length = is.tellg();
            is.seekg(0, is.beg);
            pBufferOut->resize(length);
            is.read((char*)&(*pBufferOut)[0], length);

            // Copy the URI to file name with no manipulation.
            *pFileNameOut = currPath;

            // Return indicating success.
            return true;
        }

        // Return indicating failure.
        return false;
    };
    _pRenderer->setLoadResourceFunction(loadResourceFunc);

    // Set reference flag.
    if (_pArguments->count("reference") > 0)
    {
        _pRenderer->options().setBoolean("isReferenceBSDFEnabled", true);
    }

    if (_pArguments->count("denoise") > 0)
    {
        _isDenoisingEnabled = arguments["denoise"].as<bool>();
    }
    _camera.setDimensions(_dimensions);

    // Create an Aurora environment and ground plane.
    _environmentPath = "PlasmaEnvironment";

    _pGroundPlane = _pRenderer->createGroundPlanePointer();
    _pGroundPlane->values().setBoolean("enabled", false);

    // We will set the camera view if any of the camera properties were specified as arguments.
    vec3 eye(0.0f, 0.0f, 1.0f);
    vec3 target(0.0f, 0.0f, 0.0f);
    bool shouldSetCamera = false;

    // Get the camera properties, if they were supplied as arguments.
    if (getFloat3Option("eye", eye))
    {
        shouldSetCamera = true;
    }
    if (getFloat3Option("target", target))
    {
        shouldSetCamera = true;
    }

    // Get light command line options.
    getFloat3Option("lightDir", _lightStartDirection);
    getFloat3Option("lightColor", _lightColor);
    if (arguments.count("lightIntensity"))
        _lightIntensity = arguments["lightIntensity"].as<float>();

    // Get the scene file path from the scene positional argument.
    bool bFileLoaded = false;
    if (arguments.count("scene"))
    {
        string filePath = arguments["scene"].as<string>();
        bFileLoaded     = loadSceneFile(filePath);
    }

    // Get the MaterialX file path from the mtlx argument.
    if (arguments.count("mtlx"))
    {
        string mtlxPath = arguments["mtlx"].as<string>();
        loadMaterialXFile(mtlxPath);
    }

    // If a file was not loaded, create a sample scene.
    if (!bFileLoaded)
    {
        // Create the sample scene, and return if it not successful.
        _pScene = createSampleScene(_pRenderer.get(), _sceneContents);
        if (!_pScene)
        {
            return false;
        }

        // Create empty layer array for each instance.
        _instanceLayers = vector<Layers>(_sceneContents.instances.size());

        // Update for the new scene.
        updateNewScene();

        // Fit the camera to the scene bounds, with a special direction for this scene.
        static const vec3 kDefaultDirection = normalize(vec3(0.0f, 0.0f, -1.0f));
        _camera.fit(_sceneContents.bounds, kDefaultDirection);
    }

    _pDistantLight = _pScene->addLightPointer(Aurora::Names::LightTypes::kDistantLight);

    // Get the environment map file path from the env argument.
    if (arguments.count("env"))
    {
        string envPath = arguments["env"].as<string>();
        loadEnvironmentImageFile(envPath);
    }

    // Set the camera view if needed.
    if (shouldSetCamera)
    {
        _camera.setView(eye, target);
    }

    // Set the FOV if needed.
    if (arguments.count("fov"))
    {
        float fovDeg = arguments["fov"].as<float>();
        _camera.setProjection(radians(fovDeg), 0.1f, 1.0f);
    }

    // If output is set render single frame then exit.
    if (_pArguments->count("output"))
    {
        string outputFile = arguments["output"].as<string>();
        saveImage(Foundation::s2w(outputFile), uvec2(_dimensions.x, _dimensions.y));
        AU_INFO("Output command line option is set. Rendered one image to %s, now exiting.",
            outputFile.c_str());

#if defined(INTERACTIVE_PLASMA)
        PostMessage(_hwnd, WM_CLOSE, 0, 0);
#endif
    }
#if defined(INTERACTIVE_PLASMA)
    else
    {
        // Create an Aurora window, which can be used to render into the application window.
        _pWindow = _pRenderer->createWindow(_hwnd, _dimensions.x, _dimensions.y);
        if (!_pWindow)
        {
            return false;
        }
        _pRenderer->setTargets({ { Aurora::AOV::kFinal, _pWindow } });
    }
#endif
    return true;
}

bool Plasma::addDecal(const string& decalMtlXPath)
{
    // Load the materialX file for decal.
    auto materialPath = loadMaterialXFile(decalMtlXPath);
    if (materialPath.empty())
        return false;

    // Set the current decal path.
    _decalMaterialXFilePath = decalMtlXPath;

    // Get the view matrix.
    const float* viewArray = value_ptr(_camera.viewMatrix());
    mat4 view              = glm::make_mat4(viewArray);

    // Create geometry for each instance with new UV set based on camera view.
    int instanceIndex = 0;
    for (size_t i = 0; i < _sceneContents.instances.size(); i++)
    {
        auto& inst                 = _sceneContents.instances[i];
        auto& geom                 = _sceneContents.geometry[inst.geometryPath];
        int layerIndex             = (int)_instanceLayers[instanceIndex].size();
        Aurora::Path layerGeomPath = inst.def.path + ":LayerGeom-" + to_string(layerIndex);
        _instanceLayers[instanceIndex].push_back({});
        auto& layer    = _instanceLayers[instanceIndex][layerIndex];
        layer.geomPath = layerGeomPath;
        layer.mtlPath  = materialPath;
        for (size_t j = 0; j < geom.descriptor.vertexDesc.count; j++)
        {
            // Transform position into view space.
            vec4 pos(geom.positions[j * 3 + 0], geom.positions[j * 3 + 1],
                geom.positions[j * 3 + 2], 1.0f);
            mat4 xform =
                inst.def.properties[Aurora::Names::InstanceProperties::kTransform].asMatrix4();
            pos          = pos * xform;
            vec4 projPos = view * pos;

            // Create UV based scaled and offet view coordinate.
            vec2 uv(projPos.x, projPos.y);
            uv *= 0.1;
            uv += 0.5;
            layer.uvs.push_back(uv);
        }

        // Create the Aurora geometry object.
        Aurora::GeometryDescriptor& geomDesc = layer.geomDesc;
        geomDesc.type                        = Aurora::PrimitiveType::Triangles;
        geomDesc.vertexDesc.attributes[Aurora::Names::VertexAttributes::kTexCoord0] =
            Aurora::AttributeFormat::Float2;
        geomDesc.vertexDesc.count = geom.descriptor.vertexDesc.count;
        geomDesc.indexCount       = 0;
        geomDesc.getAttributeData = [this, instanceIndex, layerIndex](
                                        Aurora::AttributeDataMap& buffers, size_t /* firstVertex*/,
                                        size_t /* vertexCount*/, size_t /* firstIndex*/,
                                        size_t /* indexCount*/) {
            auto& layer = _instanceLayers[instanceIndex][layerIndex];
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].address = layer.uvs.data();
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].size =
                layer.uvs.size() * sizeof(vec2);
            buffers[Aurora::Names::VertexAttributes::kTexCoord0].stride = sizeof(vec2);
            return true;
        };

        // Build array of geometry and material paths for layers.
        vector<Aurora::Path> layerGeomPaths;
        vector<Aurora::Path> layerMtlPaths;
        for (size_t j = 0; j < _instanceLayers[i].size(); j++)
        {
            layerGeomPaths.push_back(_instanceLayers[i][j].geomPath);
            layerMtlPaths.push_back(_instanceLayers[i][j].mtlPath);
        }

        // Create the geometry object with give path.
        _pScene->setGeometryDescriptor(layerGeomPath, geomDesc);

        // Remove the old instance.
        _pScene->removeInstance(inst.def.path);

        // The layer properties.
        Aurora::Properties newProp = inst.def.properties;
        if (!_materialXFilePath.empty())
            newProp[Aurora::Names::InstanceProperties::kMaterial] =
                "MaterialX:" + _materialXFilePath;
        newProp[Aurora::Names::InstanceProperties::kMaterialLayers] = layerMtlPaths;
        newProp[Aurora::Names::InstanceProperties::kGeometryLayers] = layerGeomPaths;

        // Create the instance.
        _pScene->addInstance(inst.def.path, inst.geometryPath, newProp);
        instanceIndex++;
    }

    _shouldRestart = true;
    return true;
}

void Plasma::updateNewScene()
{
    // Reset the sample counter, as the new scene may have very different complexity. Also reset
    // the animation timer and frame number.
    _sampleCounter.reset();
    _animationTimer.reset(!_isAnimating);
    _frameNumber = 0;

    // Set the scene on the renderer, and assign the environment and ground plane. Set the ground
    // plane position to the bottom corner, of the scene bounds.
    _pRenderer->setScene(_pScene);

    // Setup environment for new scene.
    _pScene->setEnvironmentProperties(
        _environmentPath, { { Aurora::Names::EnvironmentProperties::kBackgroundUseScreen, true } });
    _pScene->setEnvironment(_environmentPath);

    _pScene->setGroundPlanePointer(_pGroundPlane);
    _pGroundPlane->values().setFloat3("position", value_ptr(_sceneContents.bounds.min()));

    // Request a history reset for the next render if the path tracing renderer is being used, since
    // any retained history is probably invalid with a new scene.
    _pRenderer->options().setBoolean("isResetHistoryEnabled", true);

    // Set the bounds on the scene.
    const vec3& min = _sceneContents.bounds.min();
    const vec3& max = _sceneContents.bounds.max();
    _pScene->setBounds(value_ptr(min), value_ptr(max));

    // Fit the camera to the scene bounds.
    static const vec3 kDefaultDirection = normalize(vec3(0.0f, -0.5f, -1.0f));
    _camera.fit(_sceneContents.bounds, kDefaultDirection);
}

void Plasma::updateLighting()
{
    // Prepare directional light properties.
    float lightIntensity           = _isDirectionalLightEnabled ? _lightIntensity : 0.0f;
    const vec3 lightStartDirection = normalize(_lightStartDirection);
    const vec3 lightColor(::sRGBToLinear(_lightColor));

    // Get the animation elapsed time, in seconds.
    float elapsed = _animationTimer.elapsed() / 1000.0f;

    // Create a transformation matrix to rotate around the Y axis, from the animation time.
    static const float kSpinRate = 9.0f;
    static const vec3 kSpinAxis  = vec3(0.0f, 1.0f, 0.0f);
    mat4 transform               = rotate(radians(kSpinRate) * elapsed, kSpinAxis);

    // Compute a transformed light direction.
    _lightDirection = transform * vec4(lightStartDirection, 0.0f);

    // Update the environment light and background transforms.
    _pScene->setEnvironmentProperties(_environmentPath,
        { { Aurora::Names::EnvironmentProperties::kLightTransform, transform },
            { Aurora::Names::EnvironmentProperties::kBackgroundTransform, transform } });

    // Update the directional light.
    _pDistantLight->values().setFloat(Aurora::Names::LightProperties::kIntensity, lightIntensity);
    _pDistantLight->values().setFloat3(
        Aurora::Names::LightProperties::kColor, value_ptr(lightColor));
    _pDistantLight->values().setFloat3(
        Aurora::Names::LightProperties::kDirection, value_ptr(_lightDirection));
}

void Plasma::updateGroundPlane()
{
    // Set the relevant ground plane properties.
    Aurora::IValues& values = _pGroundPlane->values();
    values.setBoolean("enabled", _isGroundPlaneShadowEnabled || _isGroundPlaneReflectionEnabled);
    values.setFloat("shadow_opacity", _isGroundPlaneShadowEnabled ? 1.0f : 0.0f);
    values.setFloat("reflection_opacity", _isGroundPlaneReflectionEnabled ? 0.5f : 0.0f);
}

void Plasma::updateSampleCount()
{
    constexpr unsigned int kDebugModeErrors    = 1;
    constexpr unsigned int kDebugModeDenoising = 7;

    // Set the maximum number of samples based on the debug mode and denoising state:
    // - If denoising is enabled with a debug mode that shows denoising results, use the denoising
    //   sample count.
    // - Otherwise if the debug mode doesn't use the output (beauty) AOV, use a single sample.
    // - Otherwise use the maximum number of samples, i.e. for full path tracing.
    bool isDenoisingDebugMode = _isDenoisingEnabled &&
        (_debugMode <= kDebugModeErrors || _debugMode >= kDebugModeDenoising);
    uint32_t sampleCount = isDenoisingDebugMode ? kDenoisingSamples
                                                : (_debugMode > kDebugModeErrors ? 1 : kMaxSamples);
    _sampleCounter.setMaxSamples(sampleCount);
    _sampleCounter.reset();
}

// Updates the window.
void Plasma::update()
{
    // Prepare the performance monitor for the frame.
    _performanceMonitor.beginFrame(_shouldRestart);

    // Update lighting properties, which may have changed.
    updateLighting();

    // Get the view and projection matrices from the camera as float arrays, and set them on the
    // renderer as the camera (view).
    const float* viewArray = value_ptr(_camera.viewMatrix());
    const float* projArray = value_ptr(_camera.projMatrix());
    _pRenderer->setCamera(viewArray, projArray);

    // Render the scene, accumulating as many frames as possible within a target time, as
    // determined by the sample counter. This provides better visual results without making the
    // user wait too long, and is most effective on simple scenes.
    Foundation::CPUTimer firstFrameTimer;
    uint32_t sampleStart = 0;
    uint32_t sampleCount = _sampleCounter.update(sampleStart, _shouldRestart);
    if (sampleCount > 0)
    {
        _pRenderer->render(sampleStart, sampleCount);
    }

    // Report the time to render the first frame. This includes the first scene update, which
    // performs acceleration structure building and other potentially time-consuming work.
    if (_frameNumber == 0)
    {
        _pRenderer->waitForTask();
        ::infoMessage("First frame completed in " +
            to_string(static_cast<int>(firstFrameTimer.elapsed())) + " ms.");
    }

    // Increment the frame counter and clear the restart flag.
    _frameNumber++;
    _shouldRestart = false;

    // Update the performance monitor, which may emit a status message. If rendering is complete,
    // wait for the last task to finish, so that the performance monitor timing is accurate.
    bool isComplete = _sampleCounter.isComplete();
    if (isComplete)
    {
        _pRenderer->waitForTask();
    }
    _performanceMonitor.endFrame(isComplete, sampleCount);
}

#if defined(INTERACTIVE_PLASMA)
// Requests a display update, which ensures a paint message will be sent and processed. The
// parameter indicates whether to restart rendering from the first sample, e.g. when the camera has
// changed.
void Plasma::requestUpdate(bool shouldRestart)
{
    _shouldRestart = _shouldRestart || shouldRestart;

    // Invalidate the window to ensure that a paint message is sent.
    ::InvalidateRect(_hwnd, nullptr, FALSE);
}

// Toggles the animating state of the application.
void Plasma::toggleAnimation()
{
    // Toggle the flag and resume or suspend the animation timer as needed.
    _isAnimating = !_isAnimating;
    if (_isAnimating)
    {
        _animationTimer.resume();
    }
    else
    {
        _animationTimer.suspend();
    }

    // Request an update.
    requestUpdate();
}

// Toggles the application between a window and full screen.
void Plasma::toggleFullScreen()
{
    // Toggle the full screen state.
    _isFullScreenEnabled = !_isFullScreenEnabled;

    LONG windowStyle     = 0;
    HWND windowZ         = nullptr;
    UINT windowShowState = SW_SHOWNORMAL;
    RECT windowRect      = {};

    // Determine the desired window style, z placement, and location / dimensions, for a
    // windowed or full screen state. NOTE: For full screen display, this sets the window to a
    // full screen borderless window, rather than using a full screen exclusive mode.
    if (_isFullScreenEnabled)
    {
        // Store the current window location / dimensions and show state, i.e. the "placement."
        ::GetWindowPlacement(_hwnd, &_prevWindowPlacement);

        // Set the window style to have no border, and put the window on top.
        windowStyle = WS_OVERLAPPED;
        windowZ     = HWND_TOP;

        // Get the location / dimensions of the monitor that the window most occupies.
        HMONITOR hMonitor = ::MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFO);
        ::GetMonitorInfo(hMonitor, &monitorInfo);
        windowRect = monitorInfo.rcMonitor;
    }
    else
    {
        // Set the original window style and placement, and default Z.
        windowStyle     = WS_OVERLAPPEDWINDOW;
        windowZ         = HWND_NOTOPMOST;
        windowShowState = _prevWindowPlacement.showCmd;
        windowRect      = _prevWindowPlacement.rcNormalPosition;
    }

    // Set the computed window style, z placement, and location / dimensions on the window.
    ::SetWindowLong(_hwnd, GWL_STYLE, windowStyle);
    ::SetWindowPos(_hwnd, windowZ, windowRect.left, windowRect.top,
        windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

    // Show the window (again) so that changes take effect, with a show state.
    ::ShowWindow(_hwnd, windowShowState);
}

// Toggles vsync (vertical sync).
void Plasma::toggleVSync()
{
    // Toggle the flag and set it on the Aurora window.
    _isVSyncEnabled = !_isVSyncEnabled;
    _pWindow->setVSyncEnabled(_isVSyncEnabled);
}

// Select a different unit.
void Plasma::adjustUnit(int increment)
{
    // Increment the unit.
    _currentUnitIndex =
        Foundation::iwrap(_currentUnitIndex + increment, static_cast<int>(_units.size()));

    // Set the units option.
    _pRenderer->options().setString("units", _units[_currentUnitIndex]);

    // Request an update.
    requestUpdate();
}

// Adjust the renderer brightness multiplier based on an exposure value.
void Plasma::adjustExposure(float increment)
{
    // Apply the increment to the exposure value and use that to compute a brightness value for
    // the renderer. Specifically, the brightness is 2^exposure.
    _exposure += increment;
    vec3 brightness(pow(2.0f, _exposure));
    _pRenderer->options().setFloat3("brightness", value_ptr(brightness));

    // Request an update.
    requestUpdate();
}

// Adjust the renderer max luminance based on an exposure value.
void Plasma::adjustMaxLuminanceExposure(float increment)
{
    // Apply the increment to the exposure value for max luminance and use that and the base max
    // luminance (hardcoded) to compute a max luminance value for the renderer. Specifically,
    // the max luminance is base * 2^exposure.
    constexpr float kBaseMaxLuminance = 1000.0f;
    _maxLuminanceExposure += increment;
    float maxLuminance = kBaseMaxLuminance * pow(2.0f, _maxLuminanceExposure);
    _pRenderer->options().setFloat("maxLuminance", maxLuminance);

    // Request an update.
    requestUpdate();
}

// Displays a dialog for selecting a file to load, and loads it using the specified load
// function.
void Plasma::selectFile(
    const string& extension, const wchar_t* pFilters, const LoadFileFunction& loadFunc)
{
    // Prepare a structure for displaying a file open dialog.
    array<wchar_t, MAX_PATH> filePath = { '\0' }; // must be initialized for GetOpenFileName()
    OPENFILENAME desc                 = {};
    desc.lStructSize                  = sizeof(OPENFILENAME);
    desc.hwndOwner                    = _hwnd;
    desc.lpstrFilter                  = pFilters;
    desc.lpstrFile                    = filePath.data();
    desc.nMaxFile                     = MAX_PATH;
    desc.Flags                        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display a dialog for a user to select a file.
    if (::GetOpenFileName(&desc) == FALSE)
    {
        return;
    }

    // Get the extension of the file. If it is not valid, return.
    string foundExtension = Foundation::w2s(::PathFindExtension(filePath.data()));
    Foundation::sLower(foundExtension);
    if (extension.compare(foundExtension) != 0)
    {
        ::errorMessage("Invalid file extension; " + extension + " expected.");

        return;
    }

    // Display a wait cursor.
    // NOTE: The cursor is restored as soon as the next system message is received. So this only
    // works as desired here because the load blocks the application.
    ::SetCursor(::LoadCursor(nullptr, IDC_WAIT));

    // Load the file.
    loadFunc(Foundation::w2s(filePath.data()));
}

#endif

// Loads an environment image file with the specified path.
bool Plasma::loadEnvironmentImageFile(const string& filePath)
{
    // Create environment image using setImageFromFilePath;
    Path imagePath = "PlasmaEnvironmentImage/" + filePath;
    _pScene->setImageFromFilePath(imagePath, filePath, false, true);

    // Set light and background image in environment properties to the new image.
    _pScene->setEnvironmentProperties(_environmentPath,
        {
            { Names::EnvironmentProperties::kLightImage, imagePath },
            { Names::EnvironmentProperties::kBackgroundImage, imagePath },
            { Names::EnvironmentProperties::kBackgroundUseScreen, false },
        });

#if defined(INTERACTIVE_PLASMA)
    // Request an update.
    requestUpdate();
#endif

    return true;
}

// Loads a scene file with the specified path.
bool Plasma::loadSceneFile(const string& filePath)
{
    Foundation::CPUTimer loadTimer;

    // Get the appropriate function to load the specified scene file. If there is no appropriate
    // scene load function, return false.
    LoadSceneFunc loadSceneFunc = ::getLoadSceneFunc(filePath);
    if (!loadSceneFunc)
    {
        ::errorMessage("The file extension is not recognized.");

        return false;
    }

    // Get the directory from the file path, and set it as the current directory. This is often
    // necessary for the loaders to find adjacent material and image files.
    auto directory = filesystem::path(filePath).parent_path();
    filesystem::current_path(directory);

    // Create a new scene and load the scene file into it. If that fails, return immediately.
    _sceneContents.reset();
    Aurora::IScenePtr pScene = _pRenderer->createScene();
    if (!loadSceneFunc(_pRenderer.get(), pScene.get(), filePath, _sceneContents))
    {
        ::errorMessage("Unable to load the specified scene file: \"" + filePath + "\"");

        return false;
    }
    _instanceLayers = vector<Layers>(_sceneContents.instances.size());

    // Record the new scene and update application state for the new scene.
    _pScene = pScene;
    updateNewScene();

    // Report the load time.
    ::infoMessage("Loaded scene file \"" + filePath + "\" in " +
        to_string(static_cast<int>(loadTimer.elapsed())) + " ms.");

#if defined(INTERACTIVE_PLASMA)
    // Request an update.
    requestUpdate();
#endif

    return true;
}

void Plasma::saveImage(const wstring& filePath, const uvec2& dimensions)
{
    assert(dimensions.x > 0 && dimensions.y > 0);

    updateLighting();

    // Get the view and projection matrices from the camera as float arrays, and set them on the
    // renderer as the camera (view).
    auto* viewArray = reinterpret_cast<const float*>(&_camera.viewMatrix());
    auto* projArray = reinterpret_cast<const float*>(&_camera.projMatrix());
    _pRenderer->setCamera(viewArray, projArray);

    // Create a temporary render buffer.
    Aurora::IRenderBufferPtr pRenderBuffer = _pRenderer->createRenderBuffer(
        dimensions.x, dimensions.y, Aurora::ImageFormat::Integer_RGBA);

    // Render with the render buffer, then restore the window as the renderer's final target.
    _pRenderer->setTargets({ { Aurora::AOV::kFinal, pRenderBuffer } });
    _pRenderer->render(0, 100);
    _pRenderer->setTargets({ { Aurora::AOV::kFinal, _pWindow } });

    // Get the data from the render buffer, and save it to a PNG file with the specified path.
    size_t stride     = 0;
    const void* pData = pRenderBuffer->data(stride);
    int res = ::stbi_write_png(Foundation::w2s(filePath).c_str(), dimensions.x, dimensions.y, 4,
        pData, static_cast<int>(stride));
    AU_ASSERT(res, "Failed to write PNG: %s", Foundation::w2s(filePath).c_str());
}

Aurora::Path Plasma::loadMaterialXFile(const string& filePath)
{
    Aurora::Path materialPath = "MaterialX:" + filePath;
    // Load the MaterialX document into a string.
    ifstream mtlXStream(filePath);
    if (mtlXStream.fail())
        return "";

    string mtlXString((istreambuf_iterator<char>(mtlXStream)), istreambuf_iterator<char>());

    // Work out Autodesk Material Library path relative to Platform root.
    const string kSourceRoot = TOSTRING(PLATFORM_ROOT_PATH);
    const string mtlLibPath =
        kSourceRoot + "/Renderers/Tests/Data/Materials/AutodeskMaterialLibrary/";

    // Work out MaterialX resource path relative to Externals root.
    const string kExternalsRoot    = TOSTRING(EXTERNALS_ROOT_PATH);
    const string mtlxResourcesPath = kExternalsRoot + "/git/materialx/resources";

    // Replace the Autodesk Material Library in the loaded document.
    // In the ProteinX files this is referenced as C:/Program Files/Common Files/Autodesk Shared.
    // TODO: Should have file load callback in Aurora to avoid this.
    string processedMtlXString = regex_replace(
        mtlXString, regex("C:.Program Files.+Common Files.Autodesk Shared."), mtlLibPath);

    // Replace the MaterialX resource path in the loaded document.
    // In the MaterialX sample document this is referenced as ../../..
    // TODO: Should have file load callback in Aurora to avoid this.
    processedMtlXString =
        regex_replace(processedMtlXString, regex(R"(\.\.\/\.\.\/\.\.)"), mtlxResourcesPath);

    // Create a new material from the materialX document.
    _pScene->setMaterialType(
        materialPath, Aurora::Names::MaterialTypes::kMaterialX, processedMtlXString);

    return materialPath;
}
bool Plasma::applyMaterialXFile(const string& filePath)
{
    // Create a new material from the materialX document.
    Aurora::Path materialPath = loadMaterialXFile(filePath);
    if (materialPath.empty())
    {
        ::errorMessage("Failed to load MaterialX file: \"" + filePath + "\"");
        return false;
    }

    // Store the path used, so we can reload later.
    _materialXFilePath = filePath;

    // Apply the material to all the instances in the scene.
    for (size_t i = 0; i < _sceneContents.instances.size(); i++)
    {
        _pScene->setInstanceProperties(_sceneContents.instances[i].def.path,
            { {
                Aurora::Names::InstanceProperties::kMaterial,
                materialPath,
            } });
    }

#if defined(INTERACTIVE_PLASMA)
    // Request an update.
    requestUpdate();
#endif

    return true;
}

void Plasma::resetMaterials()
{
    // Reset the materials to their original values as loaded.
    for (size_t i = 0; i < _sceneContents.instances.size(); i++)
    {
        _pScene->setInstanceProperties(_sceneContents.instances[i].def.path,
            { {
                Aurora::Names::InstanceProperties::kMaterial,
                _sceneContents.instances[i]
                    .def.properties[Aurora::Names::InstanceProperties::kMaterial],
            } });
    }

    _materialXFilePath.clear();

#if defined(INTERACTIVE_PLASMA)
    // Request an update.
    requestUpdate();
#endif
}

#if defined(INTERACTIVE_PLASMA)
// Processes a message for the application window.
LRESULT Plasma::processMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // Quit when the window is destroyed.
    case WM_DESTROY:
        PostQuitMessage(0);
        _hwnd = nullptr;
        ::setMessageWindow(nullptr);
        return 0;

        // Handle dropping files on the window.
    case WM_DROPFILES:
        onFilesDropped(reinterpret_cast<HDROP>(wParam));
        return 0;

        // Handle key presses while the window has focus.
    case WM_KEYUP:
        onKeyPressed(wParam);
        return 0;

        // Handle mouse moving over the window.
    case WM_MOUSEMOVE:
        onMouseMoved(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), GET_KEYSTATE_WPARAM(wParam));
        return 0;

        // Handle mouse wheel rotation.
    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA, GET_KEYSTATE_WPARAM(wParam));
        return 0;

        // Handle window updates.
        // NOTE: This message will only be received when something relevant has changed, i.e. that
        // requires a new render to be performed.
    case WM_PAINT:
        // Update the window (render) and validate the window as updated (part of Begin/EndPaint()).
        PAINTSTRUCT ps;
        ::BeginPaint(_hwnd, &ps);
        update();
        ::EndPaint(_hwnd, &ps);

        // Request another update if rendering is not converged or animation is being performed.
        // For the latter, also request a restart, i.e. start rendering from the first sample.
        if (!_sampleCounter.isComplete() || _isAnimating)
        {
            requestUpdate(_isAnimating);
        }

        return 0;

        // Handle window size changes.
    case WM_SIZE:
    {
        RECT clientRect = {};
        ::GetClientRect(_hwnd, &clientRect);
        onSizeChanged(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
        return 0;
    }

    // Handle pressing ALT-ENTER, which would otherwise produce a sound.
    // NOTE: This is here only to prevent the sound, not to prevent setting a full screen state.
    // Aurora already ensures that default handling of ALT-ENTER is disabled.
    case WM_SYSCHAR:
        if (wParam == VK_RETURN && ((lParam & (1 << 29)) != 0))
        {
            return 0;
        }
    }

    return -1;
}

// Creates a window for the application.
HWND Plasma::createWindow(const uvec2& dimensions)
{
    // Define and register a window class.
    const wstring appName = Foundation::s2w(kAppName);
    const WCHAR* appNameW = appName.c_str();
    WNDCLASS wc           = {};
    wc.lpfnWndProc        = wndProc;
    wc.hInstance          = _hInstance;
    wc.lpszClassName      = appNameW;
    wc.hIcon              = ::LoadIcon(_hInstance, MAKEINTRESOURCE(IDI_ENIGMA));
    wc.hCursor            = ::LoadCursor(nullptr, IDC_ARROW);
    ::RegisterClass(&wc);

    // Specify window styles, and get the size of window with the specified client area
    // dimensions, accounting for the extra space for the window frame.
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect { 0, 0, static_cast<LONG>(dimensions.x), static_cast<LONG>(dimensions.y) };
    ::AdjustWindowRect(&rect, style, false);
    int windowWidth  = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // Create the window.
    HWND hwnd = ::CreateWindowEx(0, appNameW, appNameW, style, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight, nullptr, nullptr, _hInstance, nullptr);

    // Set the window to accept dropped files.
    ::DragAcceptFiles(hwnd, TRUE);

    return hwnd;
}

// Handles dropping of files on to the window.
void Plasma::onFilesDropped(HDROP hDrop)
{
    // Get the path of the first file dropped.
    array<wchar_t, MAX_PATH> filePathW;
    ::DragQueryFile(hDrop, 0, filePathW.data(), MAX_PATH);
    ::DragFinish(hDrop);

    // Display a wait cursor.
    // NOTE: The cursor is restored as soon as the next system message is received. So this only
    // works as desired here because the load blocks the application.
    ::SetCursor(::LoadCursor(nullptr, IDC_WAIT));

    // Get the extension of the file, and use the appropriate file load function.
    string filePath       = Foundation::w2s(filePathW.data());
    string foundExtension = Foundation::w2s(::PathFindExtension(filePathW.data()));
    Foundation::sLower(foundExtension);
    auto funcIt = _loadFileFunctions.find(foundExtension);
    if (funcIt == _loadFileFunctions.end())
    {
        ::errorMessage("Unknown file extension: " + foundExtension);
    }
    else
    {
        (*funcIt).second(filePath);
    }
}

// Handles key presses.
void Plasma::onKeyPressed(WPARAM keyCode)
{
    // Get the renderer options object.
    Aurora::IValues& options = _pRenderer->options();

    // For sequential keys starting with 0, enable the corresponding debug mode, e.g. 2 means
    // debug mode 2 (show the view depth AOV).
    constexpr int kTildeKey     = 0xC0;
    constexpr int kZeroKey      = 0x30;
    constexpr int kMaxDebugMode = 10;
    if (keyCode == kTildeKey || (keyCode >= kZeroKey && keyCode < kZeroKey + kMaxDebugMode))
    {
        // Determine and set the debug mode based on the key code:
        // - Tilde maps to mode 0.
        // - 0 key maps to mode 10.
        // - Other number keys map to modes 1 - 9.
        _debugMode = keyCode == kTildeKey
            ? 0
            : (keyCode == kZeroKey ? kMaxDebugMode : static_cast<int>(keyCode) - kZeroKey);
        options.setInt("debugMode", _debugMode);

        // Update the desired sample count as that is affected by the debug mode, and request an
        // update.
        updateSampleCount();
        requestUpdate();

        return;
    }

    switch (keyCode)
    {
    // ESC: Destroy the main window.
    case VK_ESCAPE:
        DestroyWindow(_hwnd);
        break;

    // F11: Toggle full screen display.
    case VK_F11:
        toggleFullScreen();
        break;

    // Space: Toggle animation.
    case VK_SPACE:
        toggleAnimation();
        break;

    // B: Toggle reference BSDF.
    case 0x42:
        _isReferenceBSDFEnabled = !_isReferenceBSDFEnabled;
        _pRenderer->options().setBoolean("isReferenceBSDFEnabled", _isReferenceBSDFEnabled);
        requestUpdate();
        break;

    // C: Toggle orthographic projection.
    case 0x43:
        _isOrthoProjection = !_isOrthoProjection;
        _camera.setIsOrtho(_isOrthoProjection);
        requestUpdate();
        break;

    // D: Toggle diffuse only.
    // SHIFT-D: Toggle denoising.
    case 0x44:
        if (::GetAsyncKeyState(VK_SHIFT) != 0)
        {
            _isDenoisingEnabled = !_isDenoisingEnabled;
            options.setBoolean("isDenoisingEnabled", _isDenoisingEnabled);
            updateSampleCount();
        }
        else
        {
            _isDiffuseOnlyEnabled = !_isDiffuseOnlyEnabled;
            options.setBoolean("isDiffuseOnlyEnabled", _isDiffuseOnlyEnabled);
        }
        requestUpdate();
        break;

    // CTRL-E: Select an environment image file to load.
    case 0x45:
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            LoadFileFunction func = _loadFileFunctions[".hdr"];
            selectFile(".hdr", L"HDR File\0*.hdr\0All Files\0*.*\0\0", func);
        }
        break;

    // F: Fit the view to the scene, retaining the current direction and up vectors.
    case 0x46:
        _camera.fit(_sceneContents.bounds);
        requestUpdate();
        break;

    // G: Toggle ground plane shadow.
    // SHIFT-G: Toggle ground plane reflection.
    case 0x47:
        if (::GetAsyncKeyState(VK_SHIFT) != 0)
        {
            _isGroundPlaneReflectionEnabled = !_isGroundPlaneReflectionEnabled;
        }
        else
        {
            _isGroundPlaneShadowEnabled = !_isGroundPlaneShadowEnabled;
        }
        updateGroundPlane();
        requestUpdate();
        break;

    // I: Cycle through the importance sampling modes:
    // - 0: BSDF sampling.
    // - 1: Environment light sampling.
    // - 2: Multiple importance sampling (MIS).
    case 0x49:
        _importanceSamplingMode = (_importanceSamplingMode + 1) % 3;
        _pRenderer->options().setInt("importanceSamplingMode", _importanceSamplingMode);
        requestUpdate();
        break;

    // L: Toggle the directional light.
    case 0x4c:
        _isDirectionalLightEnabled = !_isDirectionalLightEnabled;
        requestUpdate();
        break;

    // CTRL-M: Select a MaterialX file to load, and apply it to instances.
    // M: Reload the previously loaded MaterialX document.
    case 0x4d:
    {
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            LoadFileFunction func = _loadFileFunctions[".mtlx"];
            selectFile(".mtlx", L"MaterialX File\0*.mtlx\0All Files\0*.*\0\0", func);
        }
        else if (!_materialXFilePath.empty())
        {
            loadMaterialXFile(_materialXFilePath);
        }
    }
    break;

    // CTRL-O: Select a scene file to load.
    // NOTE: Currently this only supports OBJ files, but it may be expanded to other file types.
    // O: Toggle whether to treat all objects as opaque for shadows, as a performance optimization.
    case 0x4f:
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            LoadFileFunction func = _loadFileFunctions[".obj"];
            selectFile(".obj", L"OBJ File\0*.obj\0All Files\0*.*\0\0", func);
        }
        else
        {
            _isOpaqueShadowsEnabled = !_isOpaqueShadowsEnabled;
            options.setBoolean("isOpaqueShadowsEnabled", _isOpaqueShadowsEnabled);
            requestUpdate();
        }
        break;

    // R: Reset materials to original ones.
    case 0x52:
        resetMaterials();
        break;

    // S: Save the current image to a file.
    case 0x53:
        saveImage(L"capture.png", uvec2(1920, 1080));
        break;

    // T: Toggle tone mapping.
    case 0x54:
        _isToneMappingEnabled = !_isToneMappingEnabled;
        options.setBoolean("isToneMappingEnabled", _isToneMappingEnabled);
        requestUpdate();
        break;

    // V: Toggle v-sync.
    case 0x56:
        toggleVSync();
        break;

    // U: Increment units.
    case 0x55:
        adjustUnit(+1);
        break;

    // W: Add decal from materialX file.
    case 0x57:
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            LoadFileFunction func = bind(&Plasma::addDecal, this, placeholders::_1);
            selectFile(".mtlx", L"MaterialX File\0*.mtlx\0All Files\0*.*\0\0", func);
        }
        else if (!_decalMaterialXFilePath.empty())
        {
            addDecal(_decalMaterialXFilePath);
        }
        break;

    // Y: Decrement units
    case 0x59:
        adjustUnit(-1);
        break;

    // +: Increase exposure.
    // CTRL+: Increase max luminance exposure.
    case 0x6b:
    case 0xbb:
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            adjustMaxLuminanceExposure(1.0f);
        }
        else
        {
            adjustExposure(0.5f);
        }
        break;

    // -: Decrease exposure.
    // CTRL-: Decrease max luminance exposure.
    case 0x6d:
    case 0xbd:
        if (::GetAsyncKeyState(VK_CONTROL) != 0)
        {
            adjustMaxLuminanceExposure(-1.0f);
        }
        else
        {
            adjustExposure(-0.5f);
        }
        break;

    // [: Decrease max trace depth.
    case 0xdb:
        _traceDepth = glm::max(1, _traceDepth - 1);
        options.setInt("traceDepth", _traceDepth);
        requestUpdate();
        break;

    // ]: Increase max trace depth.
    case 0xdd:
        _traceDepth = glm::min(10, _traceDepth + 1);
        options.setInt("traceDepth", _traceDepth);
        requestUpdate();
        break;
    }
}

// Handles mouse moves.
void Plasma::onMouseMoved(int xPos, int yPos, WPARAM buttons)
{
    // Update the camera.
    Camera::Inputs inputs = {};
    inputs.LeftButton     = (buttons & MK_LBUTTON) != 0;
    inputs.MiddleButton   = (buttons & MK_MBUTTON) != 0;
    inputs.RightButton    = (buttons & MK_RBUTTON) != 0;
    _camera.mouseMove(xPos, yPos, inputs);

    // Request an update if the camera is dirty.
    if (_camera.isDirty())
    {
        requestUpdate();
    }
}

// Handle mouse wheel input.
void Plasma::onMouseWheel(int delta, WPARAM /*buttons*/)
{
    // Update the camera.
    Camera::Inputs inputs = {};
    inputs.Wheel          = true;
    _camera.mouseMove(0, delta, inputs);

    // Request an update if the camera is dirty.
    if (_camera.isDirty())
    {
        requestUpdate();
    }
}

// Handles window size changes.
void Plasma::onSizeChanged(UINT clientWidth, UINT clientHeight)
{
    // Do nothing if the dimensions have not changed.
    if (clientHeight == _dimensions.y && clientWidth == _dimensions.x)
    {
        return;
    }

    // If the window has a zero dimension (e.g. it is minimized), validate the window so that no
    // further paint messages are received (thus preventing unnecessary rendering) and return.
    // NOTE: Restoring the window will invalidate the window and restart the paint messages.
    if (clientWidth == 0 || clientHeight == 0)
    {
        ::ValidateRect(_hwnd, nullptr);
        return;
    }

    // Resize the Aurora window and update the camera.
    _dimensions = uvec2(clientWidth, clientHeight);
    _pWindow->resize(clientWidth, clientHeight);
    _camera.setDimensions(_dimensions);
    _performanceMonitor.setDimensions(_dimensions);

    // Request an update.
    requestUpdate();
}

// Application entry point.
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ PWSTR /*lpCmdLine*/, _In_ int /*nCmdShow*/)
{
    // Initialize memory leak detection.
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _crtBreakAlloc = XXX; <- set a memory allocation breakpoint

    // Create an application object on the stack, and run it. The Run() function returns when
    // the user quits the application, and it is destroyed.
    Plasma app(hInstance);
    gpApp       = &app;
    bool result = app.run();

    return result ? 0 : -1;
}
#else  //! INTERACTIVE_PLASMA
int main(int argc, char* argv[])
{
    // Create an application object on the stack, and run it. The run() function returns when
    // the single image rendering is finished, and the appllication object is then destroyed.
    Plasma app;
    gpApp       = &app;
    bool result = app.run(argc, argv);

    return result ? 0 : -1;
}
#endif // INTERACTIVE_PLASMA