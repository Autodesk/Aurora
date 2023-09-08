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
#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

interface IGeometry
{
    uint3 getIndices(int bufferLocation);
    float3 getPosition(int bufferLocation);
    float3 getNormal(int bufferLocation);
    float3 getTangent(int bufferLocation);
    float2 getTexCoord(int bufferLocation);
    bool hasTexCoords();
    bool hasNormals();
    bool hasTangents();
};


// Shading data for an intersection point, with interpolated values from adjacent vertices.
struct ShadingData
{
    float3 position;       // the shading position, which may be slightly above the surface
    float3 geomPosition;   // the geometric position, interpolated from adjancent vertices
    float3 normal;         // the shading normal, interpolated from adjancent vertices
    float2 texCoord;       // the shading texture coordinates, interpolated from adjancent vertices
    float3 tangent;        // the shading tangent (X), interpolated from adjacent vertices
    float3 bitangent;      // the shading bitangent (Y), interpolated from adjacent vertices
    float3 objectNormal;   // the shading position in object space (not world space). Used only in
                           // MaterialX generated code.
    float3 objectPosition; // the shading normal in object space (not world space). Used only in
                           // MaterialX generated code.
};

// Computes the barycentric coordinates from the specified triangle intersection.
float3 computeBarycentrics(BuiltInTriangleIntersectionAttributes attribs)
{
    float2 bary2 = attribs.barycentrics;
    return float3(1.0f - bary2.x - bary2.y, bary2.x, bary2.y);
}

// Projects the specified position (point) onto the plane with the specified origin and normal.
float3 projectOnPlane(float3 position, float3 origin, float3 normal)
{
    return position - dot(position - origin, normal) * normal;
}

// Computes the shading position of the specified geometric position and vertex positions and
// normals. For a triangle with normals describing a convex surface, this point will be slightly
// above the surface. For a concave surface, the geometry position is used directly.
// NOTE: The difference between the shading position and geometry position is significant when
// casting shadow rays. If the geometric position is used, a triangle may fully shadow itself when
// it should be partly lit based on the shading normals; this is the "shadow terminator" problem.
float3 computeShadingPosition(float3 geomPosition, float3 shadingNormal, float3 positions[3],
    float3 normals[3], float3 barycentrics)
{
    // Project the geometric position (inside the triangle) to the planes defined by the vertex
    // positions and normals.
    float3 p0 = projectOnPlane(geomPosition, positions[0], normals[0]);
    float3 p1 = projectOnPlane(geomPosition, positions[1], normals[1]);
    float3 p2 = projectOnPlane(geomPosition, positions[2], normals[2]);

    // Interpolate the projected positions using the barycentric coordinates, which gives the
    // shading position.
    float3 shadingPosition = p0 * barycentrics.x + p1 * barycentrics.y + p2 * barycentrics.z;

    // Return the shading position for a convex triangle, where the shading point is above the
    // triangle based on the shading normal. Otherwise use the geometric position.
    bool convex = dot(shadingPosition - geomPosition, shadingNormal) > 0.0f;
    return convex ? shadingPosition : geomPosition;
}

// Computes the shading data (e.g. interpolated vertex data) for the triangle with the specified
// (primitive) index, at the specified barycentric coordinates.
ShadingData computeShadingData<GeometryType : IGeometry>(
    GeometryType geometry, uint triangleIndex, float3 barycentrics, float3x4 objToWorld)
{
    // Initialize a result structure.
    ShadingData shading;
    shading.position     = float3(0.0f, 0.0f, 0.0f);
    shading.geomPosition = float3(0.0f, 0.0f, 0.0f);
    shading.normal       = float3(0.0f, 0.0f, 0.0f);
    shading.texCoord     = float2(0.0f, 0.0f);
    shading.tangent      = float3(0.0f, 0.0f, 0.0f);

    // Load the indices for the vertices of the triangle with the specified index.
    uint3 indices = geometry.getIndices(triangleIndex);

    // Compute interpolated values for each of the data channels at the specified barycentric
    // coordinates. As byte address buffers, the data arrays must be indexed by byte, and loaded
    // as 32-bit unsigned ints, which are then read as floats.
    float3 positions[3];
    float3 normals[3];
    float3 tangents[3];
    for (uint i = 0; i < 3; i++)
    {
        positions[i] = (geometry.getPosition(indices[i]));

        // Accumulate the positions, weighted by the barycentric coordinates.
        // NOTE: An alternative is to compute the position with ray intrinsics, as follows:
        // WorldRayOrigin() + WorldRayDirection() * RayTCurrent(). However this will be less
        // accurate due to floating-point precision; see "Ray Tracing Gems" chapter 6.
        shading.geomPosition += positions[i] * barycentrics[i];
        
        // Accumulate the optional normals, weighted by the barycentric coordinates.
        if (geometry.hasNormals())
        {
            normals[i] = (geometry.getNormal(indices[i]));
            shading.normal += normals[i] * barycentrics[i];
        }

        // Accumulate the optional normals, weighted by the barycentric coordinates.
        if (geometry.hasTangents())
        {
            tangents[i] = (geometry.getTangent(indices[i]));
            shading.tangent += tangents[i] * barycentrics[i];
        }

        // Accumulate the optional texture coordinates, weighted by the barycentric coordinates.
        if (geometry.hasTexCoords())
        {
            shading.texCoord += (geometry.getTexCoord(indices[i])) * barycentrics[i];
        }
    }

    // Normalize the interpolated (shading) normal. If no normals are provided, compute a geometric
    // normal using the vertex positions; this will yield flat shading.
    // NOTE: If a provided normal has zero length, then the normal will have undefined components
    // (likely NaN), leading to rendering artifacts (likely black pixels) along any path that
    // accesses the normal.
    shading.objectNormal = normalize(geometry.hasNormals()
            ? shading.normal
            : cross(positions[1] - positions[0], positions[2] - positions[0]));

    // Compute the shading position from the geometry position and vertex data. If no normals are
    // provided, use the geometric position.
    shading.objectPosition = geometry.hasNormals() ? computeShadingPosition(shading.geomPosition,
                                                   shading.normal, positions, normals, barycentrics)
                                             : shading.geomPosition;

    // Transform the position, geometric position, and normal to world space.
    // NOTE: Renormalize the normal as the object-to-world matrix can have a scale.
    shading.position     = mul(objToWorld, float4(shading.objectPosition, 1.0f));
    shading.geomPosition = mul(objToWorld, float4(shading.geomPosition, 1.0f));
    shading.normal       = normalize(mul((float3x3)objToWorld, shading.objectNormal));
    
    // Arbritary up vector used to calculate tangents, if none provided.
    float3 up =
        abs(shading.normal.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);

    // Compute arbritary shading tangent from up vector, or use provided one transformed in world space.
    shading.tangent = normalize(geometry.hasTangents() ? normalize(mul((float3x3)objToWorld, shading.tangent))
                                            : cross(shading.normal, up));

    // Generate bitangent from normal and tangent.
    shading.bitangent = computeBitangent(shading.normal, shading.tangent);

    return shading;
}

#endif // __GEOMETRY_H__