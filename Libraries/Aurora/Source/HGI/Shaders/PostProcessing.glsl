// Post processing data UBO.
// NOTE should be passed as push constants, but this is currently broken in HGI.
layout(binding = 3) layout(std140) uniform PostProcesssing
{
    vec3 brightness;
    int debugMode;
    vec2 range;
    bool isDenoisingEnabled;
    bool isToneMappingEnabled;
    bool isGammaCorrectionEnabled;
    bool isAlphaEnabled;
} gSettings;

// Get texture UV from input coordinate.
vec2 GetTexCoords(ivec2 outCoords)
{
    vec2 outDims = vec2(HgiGetSize_outTexture());
    // apply a (0.5, 0.5) offset to use pixel centers and not pixel corners
    vec2 texCoords = (vec2(outCoords) + vec2(0.5, 0.5)) / outDims;
    return texCoords;
}

// Applies the ACES filmic tone mapping curve to the color.
// NOTE: Based on https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve.
vec3 toneMapACES(vec3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Converts from linear color space to sRGB (gamma correction) for display.
// NOTE: Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html.
vec3 linearTosRGB(vec3 color)
{
    vec3 sq1 = sqrt(color);
    vec3 sq2 = sqrt(sq1);
    vec3 sq3 = sqrt(sq2);

    return 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * color;
}

void main(void)
{
    // Sample input accumulation texture.
    ivec2 outCoords = ivec2(hd_GlobalInvocationID.xy);
    vec2 texCoords = GetTexCoords(outCoords);
    vec3 color       = HgiTextureLod_accumulationTexture(texCoords, 0.0).rgb;

    // Apply brightness.
    color *= gSettings.brightness;

    // Apply ACES tone mapping or simple saturation.
    if (gSettings.isToneMappingEnabled)
    {
        color = toneMapACES(color);
    }

    // Apply gamma correction.
    // NOTE: Gamma correction must be performed here as UAV textures don't support sRGB write.
    if (gSettings.isGammaCorrectionEnabled)
    {
        color = linearTosRGB(clamp(color,0.0,1.0));
    }

    // For now just return input directly.   Force alpha to one.
    // TODO: Correct alpha output.
    HgiSet_outTexture(outCoords, vec4(color,1.0));
}
