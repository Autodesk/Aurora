
//        HgiMetalArgumentIndexConstants = 27,
//        HgiMetalArgumentIndexSamplers = 28,
//        HgiMetalArgumentIndexTextures = 29,
//        HgiMetalArgumentIndexBuffers = 30,
        
struct Uniforms {
    unsigned int width, height, frameIndex;
};

struct Textures {
    texture2d<float, access::read_write> tex_0;
    texture2d<float, access::read_write> tex_1;
    texture2d<float, access::read_write> tex_2;
    texture2d<float, access::read_write> tex_3;
    texture2d<float, access::read_write> tex_4;
    texture2d<float, access::read_write> tex_5;
    texture2d<float, access::read_write> tex_6;
    texture2d<float, access::read_write> tex_7;
    texture2d<float, access::read_write> tex_8;
    texture2d<float, access::read_write> tex_9;
};

struct Samplers {
    sampler smp_0;
};

struct Buffers {
    device void* buf_0;
    device void* buf_1;
    constant FrameData* frameData;
    device void* buf_3;
    constant SampleData* sampleData;
    constant EnvironmentData* environmentData;
    device void* buf_6;
//"    constant MTLAccelerationStructureInstanceDescriptor *instances;
};

template<typename T>
inline T interpolateVertexAttribute(thread T* attributes, float2 uv) {
    const T T0 = attributes[0];
    const T T1 = attributes[1];
    const T T2 = attributes[2];
    return (1.f - uv.x - uv.y) * T0 + uv.x * T1 + uv.y * T2;
}

enum TriangleFlags {
    TRIANGLE_FLAGS_EMISSION = 0x1,
};

struct Triangle {
    float3 normals[3];
    float4 diffuse;
    float smoothness;
    float emissiveStrength;
    uint32_t flags;
};

///////////////////////////////////
// FROM Globals.slang
///////////////////////////////////

// Math constants.
#define M_PI 3.1415926535897932384626433832795f
#define M_PI_INV 1.0 / 3.1415926535897932384626433832795
#define M_FLOAT_EPS 0.000001f
#define M_RAY_TMIN 0.01f

///////////////////////////////////
// FROM Random.slang
///////////////////////////////////

// State used for random number generation (RNG).
struct Random
{
    uint2 state;
};

// Initializes a random number generator for PCG2D.
Random pcg2DInit(uint sampleIndex, uint2 screenSize, uint2 screenCoords)
{
    Random rng;

    // Initialize the state based on the pixel location and sample index, to prevent correlation
    // across space and time.
    // NOTE: Unlike LCG, PCG2D does behave well with consecutive seeds, so the seed does not need to
    // be scrambled. See https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering.
    rng.state.x = screenCoords.y * screenSize.x + screenCoords.x;
    rng.state.y = sampleIndex;

    return rng;
}


// Generates two random 32-bit integers from seed values, using a two-dimensional permuted
// congruential generator (PCG).
// NOTE: Based on "Hash Functions for GPU Rendering" @ http://www.jcgt.org/published/0009/03/02.
uint2 pcg2D(thread uint2& state)
{
    // Retain the current state, and advance to the next state with a simple LCG.
    uint2 v = state;
    state = state * 1664525u + 1013904223u;

    // Apply the PCG2D transformation to the current state to produce the result pair of values.
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v   = v ^ (v >> 16u);
    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;
    v   = v ^ (v >> 16u);

    return v;
}

// Computes the next PCG2D value pair, updating the specified RNG.
float2 pcg2DNext2D(thread Random& rng)
{
    rng.state = pcg2D(rng.state);

    // Produce floating-point numbers in the range [0.0, 1.0).
    return float2(float2(rng.state) / float2(0xFFFFFFFFu));
}

// Generates two uniformly distributed numbers in the range [0.0, 1.0), using the specified random
// number generator.
// NOTE: The two numbers may be correlated to provide an even distribution across the 2D range,
// which is why this is a single 2D function instead of two calls to a 1D function.
float2 random2D(thread Random& rng)
{
    return pcg2DNext2D(rng);
}

///////////////////////////////////
// FROM Sampling.slang
///////////////////////////////////

// Generates a uniformly distributed random direction.
float3 sampleUniformDirection(float2 random, thread float& pdf) {
    // Create a point on the unit sphere, from the uniform random variables. Since this is a unit
    // sphere, this can also be used as a direction.
    // NOTE: See "Ray Tracing Gems" section 16.5 for details.
    float cosTheta   = 1.0f - 2.0f * random[1];
    float sinTheta   = sqrt(1.0f - cosTheta * cosTheta);
    float phi        = 2.0f * M_PI * random[0];
    float3 direction = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // The PDF is uniform, at one over the area, i.e. the total solid angle of 4*Pi.
    pdf = 0.25f * M_PI_INV;

    return direction;
}

// Generates a random direction in the cosine-weighted hemisphere above the specified normal. This
// provides a PDF value ("probability density function") which is the *relative* probability that
// the returned direction will be chosen.
float3 sampleHemisphere(float2 random, float3 normal, thread float& pdf) {
    // Uniformly sample a direction, i.e. over a sphere.
    float3 direction = sampleUniformDirection(random, pdf);

    // To transform that into a sample from a cosine-weighted hemisphere over the normal, treat the
    // sphere as tangent to the surface: add the normal to the direction and normalize it. The PDF
    // is cos(theta) / PI, so use a dot product to compute cos(theta).
    // NOTE: See "Ray Tracing in One Weekend" for details.
    direction = normalize(normal + direction);
    pdf       = dot(normal, direction) * M_PI_INV;

    return direction;
}

///////////////////////////////////
// FROM RayTrace.slang
///////////////////////////////////

void computeCameraRay(float2 screenCoords, float2 screenSize, float4x4 invView, float2 viewSize,
    bool isOrtho, float focalDistance, float lensRadius, thread Random& randState, thread float3& origin,
    thread float3& direction)
{
    // Apply a random offset to the screen coordinates, for antialiasing. Convert the screen
    // coordinates to normalized device coordinates (NDC), i.e. the range [-1, 1] in X and Y. Also
    // flip the Y component, so that +Y is up.

    screenCoords += random2D(randState);
    float2 ndc = (screenCoords / screenSize) * 2.f - 1.f;
    ndc.y      = -ndc.y;

    // Get the world-space orientation vectors from the inverse view matrix.
    float3 right =  invView[0].xyz;  // right: row 0
    float3 up    =  invView[1].xyz;  // up: row 1
    float3 front = -invView[2].xyz; // front: row 2, negated for RH coordinates

    // Build a world-space offset on the view plane, based on the view size and the right and up
    // vectors.
    float2 size            = viewSize * 0.5f;
    float3 offsetViewPlane = size.x * ndc.x * right + size.y * ndc.y * up;

    // Compute the ray origin and direction:
    // - Direction: For orthographic projection, this is just the front direction (i.e. all rays are
    //   parallel). For perspective, it is the normalized combination of the front direction and the
    //   view plane offset.
    // - Origin: For orthographic projection, this is the eye position (row 3 of the view matrix),
    //   translated by the view plane offset. For perspective, it is just the eye position.
    //
    // NOTE: It is common to \"unproject\" a NDC point using the view-projection matrix, and subtract
    // that from the eye position to get a direction. However, this is numerically unstable when the
    // eye position has very large coordinates and the projection matrix has small (nearby) clipping
    // distances. Clipping is not relevant for ray tracing anyway.
    if (isOrtho) {
        direction = front;
        origin    = invView[3].xyz + offsetViewPlane;
    }
    else {
        direction = normalize(front + offsetViewPlane);
        origin    = invView[3].xyz;
    }

    // Adjust the ray origin and direction if depth of field is enabled. The ray must pass through
    // the focal point (along the original direction, at the focal distance), with an origin that
    // is offset on the lens, represented as a disk.
//"            if (lensRadius > 0.0f)
//"            {
//"                float3 focalPoint   = origin + direction * focalDistance;
//"                float2 originOffset = sampleDisk(random2D(rng), lensRadius);
//"                origin              = origin + originOffset.x * right + originOffset.y * up;
//"                direction           = normalize(focalPoint - origin);
//"            }
}

inline float3 interpolateSkyColor(float3 ray) {
    float t = mix(ray.y, 1.0f, 0.5f);
    return mix(float3(1.0f, 1.0f, 1.0f), float3(0.45f, 0.65f, 1.0f), t);
}

float4 tracePath(thread Random& randState, float3 rayOrigin, float3 rayDirection, int depth, instance_acceleration_structure accelerationStructure, intersection_function_table<triangle_data, instancing> intersectionFunctionTable) {
 
    float3 incomingLight    = float3(0.f, 0.f, 0.f);
    float3 rayColour        = float3(1.f, 1.f, 1.f);
    
    bool continuePath;
    do {
        continuePath = false;
        
        ray ray;
        ray.origin = rayOrigin;
        ray.direction = rayDirection;
        ray.min_distance = 0.001f;
        ray.max_distance = 1000.f;
        
        intersector<triangle_data, instancing, max_levels<2>> i;
        i.accept_any_intersection(false);
        i.assume_geometry_type(geometry_type::triangle);
        i.force_opacity(forced_opacity::opaque);
        typename intersector<triangle_data, instancing, max_levels<2>>::result_type intersection;
        intersection = i.intersect(ray, accelerationStructure, 2);

        if(intersection.type == intersection_type::none) {
            float3 skyColour = interpolateSkyColor(ray.direction);//float3(0.95f, 0.95f, 0.95f);
            incomingLight += skyColour * rayColour;
        }
        else {
            Triangle triangle = *(const device Triangle*)intersection.primitive_data;
            float2 barycentricCoords = intersection.triangle_barycentric_coord;
            float3 normal = interpolateVertexAttribute(triangle.normals, barycentricCoords);
            continuePath = true;

            float smoothness       = triangle.smoothness;
            float emissiveStrength = triangle.emissiveStrength;
            float3 materialColour  = triangle.diffuse.xyz;
            if(triangle.flags & TRIANGLE_FLAGS_EMISSION) {
                rayColour = materialColour = float3(1.f, 1.f, 1.f);
                continuePath = false;
            }

            rayOrigin = rayOrigin + (intersection.distance * rayDirection);
            float pdf = 1.f;
            float3 diffuseDirection = sampleHemisphere(random2D(randState), normal, pdf);
            float3 specularDirection = reflect(rayDirection, normal);
            rayDirection = mix(diffuseDirection, specularDirection, smoothness);
            float3 emittedLight = materialColour * emissiveStrength;
            incomingLight += emittedLight * rayColour;
            rayColour *= materialColour;

            depth--;
        }
        
    } while(depth > 0 && continuePath);
    
    return float4(incomingLight, 1.f);
}

kernel void RayGenShader(
     uint2                                                  tid                       [[thread_position_in_grid]],
     instance_acceleration_structure                        accelerationStructure     [[buffer(0)]],
     constant Uniforms&                                     uniformBuf                [[buffer(27)]],
     constant Samplers&                                     samplerBuf                [[buffer(28)]],
     constant Textures&                                     textureBuf                [[buffer(29)]],
     constant Buffers&                                      bufferBuf                 [[buffer(30)]],
     intersection_function_table<triangle_data, instancing> intersectionFunctionTable [[buffer(5)]],
     visible_function_table<HandlerFuncSig>                 hitTable                  [[buffer(6)]],
     visible_function_table<HandlerFuncSig>                 missTable                 [[buffer(7)]]
)
{
    texture2d<float, access::read_write> dstTex             = textureBuf.tex_1;
    texture2d<float, access::read_write> accumulationTex    = textureBuf.tex_9;
    constant Uniforms&  uniforms = uniformBuf;

//    if (tid.x < uniforms.width && tid.y < uniforms.height)
    {
        constant FrameData& gFrameData = *bufferBuf.frameData;
    
        uint2 screenSize   = uint2(dstTex.get_width(), dstTex.get_height());
        uint2 screenCoords = tid.xy;

        // init rand
        Random randState = pcg2DInit(gFrameData.frameIndex, screenSize, screenCoords);

        // primary ray direction
        float3 origin, dir;
        computeCameraRay(float2(screenCoords), float2(screenSize), gFrameData.cameraInvView, gFrameData.viewSize,
            gFrameData.isOrthoProjection, gFrameData.focalDistance, gFrameData.lensRadius, randState, origin,
            dir);

        // path trace
        float4 accumulatedColour = tracePath(randState, origin, dir, gFrameData.traceDepth, accelerationStructure, intersectionFunctionTable);

        // accumulate
        if(gFrameData.frameIndex > 0) {
            float4 prevColour = accumulationTex.read(tid);
            prevColour *= gFrameData.frameIndex;
            
            accumulatedColour += prevColour;
            accumulatedColour /= (gFrameData.frameIndex + 1);
        }

        dstTex.write(accumulatedColour, tid);
    }
}
