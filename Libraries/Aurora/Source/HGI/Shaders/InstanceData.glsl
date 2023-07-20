
// Extensions required (should be in prefix)
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require

// Buffer types for vertex data.
layout(buffer_reference, std430, buffer_reference_align=4, scalar) buffer Indices     { uint i[]; };
layout(buffer_reference, std430, buffer_reference_align=4, scalar) buffer Positions   { vec3 v[]; };
layout(buffer_reference, std430, buffer_reference_align=4, scalar) buffer Normals     { vec3 n[]; };
layout(buffer_reference, std430, buffer_reference_align=4, scalar) buffer Tangents    { vec3 tn[]; };
layout(buffer_reference, std430, buffer_reference_align=4, scalar) buffer TexCoords   { vec2 t[]; };

// Buffer type for material.
layout(buffer_reference, std430, scalar) buffer Materials   { MaterialConstants_0 m[]; };

// Array of textures and samplers for all instances.
layout(binding = 3) uniform sampler2D textureSamplers[];

// Shader record for hit shaders. Must match HitGroupShaderRecord struct in HGIScene.h.
layout(shaderRecordEXT, std430) buffer InstanceShaderRecord
{
    // Geometry data.
    Indices indices;
    Positions positions;
    Normals normals;
    Tangents tangents;
    TexCoords texcoords;

    // Material data.
    Materials material;

    // Index into texture sampler array for material's textures.
    int baseColorTextureIndex;
    int specularRoughnessTextureIndex;
    int normalTextureIndex;
    int opacityTextureIndex;

    // Geometry flags.
    uint hasNormals;
    uint hasTangents;
    uint hasTexCoords;
} instance;


// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
uvec3 getIndicesForTriangle_0(int triangleIndex) {
    uint v0 = instance.indices.i[triangleIndex*3+0];
    uint v1 = instance.indices.i[triangleIndex*3+1];
    uint v2 = instance.indices.i[triangleIndex*3+2];

    return uvec3(v0,v1,v2);
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
vec3 getPositionForVertex_0(int vertexIndex) {
    return instance.positions.v[vertexIndex];
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
vec3 getNormalForVertex_0(int vertexIndex) {
    return instance.normals.n[vertexIndex];
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
vec3 getTangentForVertex_0(int vertexIndex) {
    return instance.tangents.tn[vertexIndex];
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
vec2 getTexCoordForVertex_0(int vertexIndex) {
    return instance.texcoords.t[vertexIndex];
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
bool instanceHasNormals_0() {
    return instance.hasNormals!=0;
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
bool instanceHasTangents_0() {
    return instance.hasTangents!=0;
}

// Implementation for forward declared geometry accessor function in PathTracingCommon.slang.
bool instanceHasTexCoords_0() {
    return instance.hasTexCoords!=0;
}

// Implementation for forward declared material accessor function in Material.hlsli.
MaterialConstants_0 getMaterial_0() {
    return instance.material.m[0];
}

// Implementation for forward declared texture sample function in Material.hlsli.
vec4 sampleBaseColorTexture_0(vec2 uv, float level) {
    return texture(textureSamplers[nonuniformEXT(instance.baseColorTextureIndex)], uv);
}

// Implementation for forward declared texture sample function in Material.hlsli.
vec4 sampleSpecularRoughnessTexture_0(vec2 uv, float level) {
    return texture(textureSamplers[nonuniformEXT(instance.specularRoughnessTextureIndex)], uv);
}

// Implementation for forward declared texture sample function in Material.hlsli.
vec4 sampleEmissionColorTexture_0(vec2 uv, float level) {
    // TODO: Implement emission color in HGI backend.
    return vec4(0,0,0,0);
}

// Implementation for forward declared texture sample function in Material.hlsli.
vec4 sampleNormalTexture_0(vec2 uv, float level) {
    return texture(textureSamplers[nonuniformEXT(instance.normalTextureIndex)], uv);
}

// Implementation for forward declared texture sample function in Material.hlsli.
vec4 sampleOpacityTexture_0(vec2 uv, float level) {
    return texture(textureSamplers[nonuniformEXT(instance.opacityTextureIndex)], uv);
}