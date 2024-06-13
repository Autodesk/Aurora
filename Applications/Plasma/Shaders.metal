
// File for Metal kernel and shader functions

#include <metal_stdlib>
#include <simd/simd.h>

// Including header shared between this Metal shader code and Swift/C code executing Metal API commands
#import "ShaderTypes.h"

using namespace metal;

constant float2 quadVertex[] = {
    float2(-1, -1),
    float2(-1,  1),
    float2( 1,  1),
    float2(-1, -1),
    float2( 1,  1),
    float2( 1, -1)
};

typedef struct {
    float4 position [[position]];
    float2 uv;
} PostColorInOut;

vertex PostColorInOut pass_through_vertex(unsigned short vid [[vertex_id]]) {
    float2 position = quadVertex[vid];
    PostColorInOut out;
    out.position = float4(position, 0, 1);
    out.uv = position * 0.5f + 0.5f;
    out.uv.y = 1.0f - out.uv.y;
    return out;
}

fragment float4 pass_through_fragment(PostColorInOut in [[stage_in]],
                                      texture2d<float> texture [[texture(0)]]) {
    constexpr sampler colorSampler(min_filter::nearest, mag_filter::nearest, mip_filter::none);
    return texture.sample(colorSampler, in.uv);
}
