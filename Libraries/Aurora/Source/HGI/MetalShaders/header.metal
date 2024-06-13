#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;
using namespace raytracing;

float3 hsv2rgb(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
struct RayPayload
{
    int rayId;
};
using HandlerFuncSig = int(ray, thread RayPayload&);

    // Structure representing a single distant light.
    // Must match GPU struct in Frame.slang
    struct DistantLight
    {
        // Light color (in RGB) and intensity (in alpha channel.)
        float4 colorAndIntensity;
        // Direction of light (inverted as expected by shaders.)
        packed_float3 direction = packed_float3(0, 0, 1);
        // The light size is converted from a diameter in radians to the cosine of the radius.
        float cosRadius = 0.0f;
    };

    struct LightData
    {
        // Array of distant lights, only first distantLightCount are used.
        DistantLight distantLights[4];

        // Number of active distant lights.
        int distantLightCount = 0;

        // Explicitly pad struct to 16-byte boundary.
        int pad[3];
    };

    struct FrameData
    {
        // The view-projection matrix.
        float4x4 cameraViewProj;

        // The inverse view matrix, also transposed. The *rows* must have the desired vectors:
        // right, up, front, and eye position. HLSL array access with [] returns rows, not columns,
        // hence the need for the matrix to be supplied transposed.
        float4x4 cameraInvView;

        // The dimensions of the view (in world units) at a distance of 1.0 from the camera, which
        // is useful to build ray directions.
        float2 viewSize;

        // Whether the camera is using an orthographic projection. Otherwise a perspective
        // projection is assumed.
        int isOrthoProjection;

        // The distance from the camera for sharpest focus, for depth of field.
        float focalDistance;

        // The diameter of the lens for depth of field. If this is zero, there is no depth of field,
        // i.e. pinhole camera.
        float lensRadius;

        // The size of the scene, specifically the maximum distance between any two points in the
        // scene.
        float sceneSize;

        // Whether shadow evaluation should treat all objects as opaque, as a performance
        // optimization.
        int isOpaqueShadowsEnabled;

        // Whether to write the NDC depth result to an output texture.
        int isDepthNDCEnabled;

        // Whether to render the diffuse material component only.
        int isDiffuseOnlyEnabled;

        // Whether to display shading errors as bright colored samples.
        int isDisplayErrorsEnabled;

        // Whether denoising is enabled, which affects how path tracing is performed.
        int isDenoisingEnabled;

        // Whether to write the AOV data required for denoising.
        int isDenoisingAOVsEnabled;

        // The maximum recursion level (or path length) when tracing rays.
        int traceDepth;

        // The maximum luminance for path tracing samples, for simple firefly clamping.
        float maxLuminance;

        // Pad to 16 byte boundary.
        float2 _padding1;

        // Current light data for scene (duplicated each frame in flight.)
        LightData lights;

        // frameIndex
        int frameIndex;
    };

    // Sample settings GPU data.
    struct SampleData
    {
        // The sample index (iteration) for the frame, for progressive rendering.
        uint sampleIndex;

        // An offset to apply to the sample index for seeding a random number generator.
        uint seedOffset;
    };

    struct EnvironmentData
    {
        packed_float3 lightTop;
        float _padding1;
        packed_float3 lightBottom;
        float lightTexLuminanceIntegral;
        float4x4 lightTransform;
        float4x4 lightTransformInv;
        packed_float3 backgroundTop;
        float _padding3;
        packed_float3 backgroundBottom;
        float _padding4;
        float4x4 backgroundTransform;
        int backgroundUseScreen;
        int hasLightTex;
        int hasBackgroundTex;
    };
