// Sample data UBO.
// NOTE should be passed as push constants, but this is currently broken in HGI.
layout(binding = 3) layout(std140) uniform SampleData
{
    int sampleIndex;
    int seedOffset;
} gSampleData;

// Get texture UV from input coordinate.
vec2 GetTexCoords(ivec2 outCoords)
{
    vec2 outDims = vec2(HgiGetSize_outTexture());
    // apply a (0.5, 0.5) offset to use pixel centers and not pixel corners
    vec2 texCoords = (vec2(outCoords) + vec2(0.5, 0.5)) / outDims;
    return texCoords;
}

void main(void)
{
    // Sample input direct light texture.
    ivec2 outCoords = ivec2(hd_GlobalInvocationID.xy);
    vec2 texCoords  = GetTexCoords(outCoords);
    vec4 result     = HgiTextureLod_directLightTexture(texCoords, 0.0);
    vec3 direct     = result.rgb;

    // For all samples except the first, combine direct light texture with previous frame.
    if (gSampleData.sampleIndex > 0)
    {
        // Get result for previous frame.
        vec4 prevResult = HgiTextureLod_accumulationTexture(texCoords, 0.0);
        
        // Blend between the previous result based on sample index.
        float t = 1.0 / (gSampleData.sampleIndex + 1);
        result = mix(prevResult, result, t);
    }

    // Write result back to accumulation.
    HgiSet_outTexture(outCoords, result);
}
