//
// Copyright 2023 by Autodesk, Inc.  All rights reserved.
//
// This computer source code and related instructions and comments
// are the unpublished confidential and proprietary information of
// Autodesk, Inc. and are protected under applicable copyright and
// trade secret law.  They may not be disclosed to, copied or used
// by any third party without the prior written consent of Autodesk, Inc.
//

#include <stddef.h>
#include <cstring>

#include <Aurora/Foundation/Geometry.h>

#include <glm/glm.hpp>

using namespace glm;

namespace Aurora
{
namespace Foundation
{

void calculateNormals(size_t vertexCount, const float* vertex, size_t triangleCount,
    const unsigned int* indices, float* normalOut)
{
    const bool angleWeighted = true;
    float angle1             = 1.0f;
    float angle2             = 1.0f;
    float angle3             = 1.0f;

    for (int face = 0; face < triangleCount; ++face)
    {
        const unsigned int& ip1 = indices[face * 3];
        const unsigned int& ip2 = indices[(face * 3) + 1];
        const unsigned int& ip3 = indices[(face * 3) + 2];

        vec3 p1(*(vertex + (ip1 * 3)), *(vertex + (ip1 * 3) + 1), *(vertex + (ip1 * 3) + 2));
        vec3 p2(*(vertex + (ip2 * 3)), *(vertex + (ip2 * 3) + 1), *(vertex + (ip2 * 3) + 2));
        vec3 p3(*(vertex + (ip3 * 3)), *(vertex + (ip3 * 3) + 1), *(vertex + (ip3 * 3) + 2));
        const vec3 vA = p3 - p1;
        const vec3 vB = p2 - p1;

        // compute the face normal
        const vec3 nrm = normalize(cross(vB, vA));
        vec3* normal   = nullptr;

        if (angleWeighted)
        {
            // compute the face angle at a vertex and weight the normals based on angle.
            const vec3 v31 = normalize(vA);
            const vec3 v21 = normalize(vB);
            const vec3 v32 = normalize(p3 - p2);
            vec3 _v31;
            _v31.x = -v31.x;
            _v31.y = -v31.y;
            _v31.z = -v31.z;
            vec3 _v21;
            _v21.x = -v21.x;
            _v21.y = -v21.y;
            _v21.z = -v21.z;
            vec3 _v32;
            _v32.x = -v32.x;
            _v32.y = -v32.y;
            _v32.z = -v32.z;

            const float d1 = dot(v31, v21);
            const float d2 = dot(v32, _v21);
            const float d3 = dot(_v31, _v32);

            angle1 = std::abs(std::acos(d1));
            angle2 = std::abs(std::acos(-d2));
            angle3 = std::abs(std::acos(d3));
        }

        // for each vertex, the normals are summed; this code assumes
        // that the mesh is supposed to be smooth - there is no
        // separation of vertices due to crease angle.
        normal = (vec3*)(normalOut + (ip1 * 3));
        *(normal) += (nrm * angle1);

        normal = (vec3*)(normalOut + (ip2 * 3));
        *(normal) += nrm * angle2;

        normal = (vec3*)(normalOut + (ip3 * 3));
        *(normal) += nrm * angle3;
    }

    // normalize all the summed up normals
    for (int i = 0; i < vertexCount; i++)
    {
        vec3* curPos = (vec3*)(normalOut + (i * 3));
        vec3 n       = normalize(*curPos);
#ifdef _WIN32
        ::memcpy_s(normalOut + i * 3, vertexCount * 3 * sizeof(float), &n, sizeof(vec3));
#else
        std::memcpy(normalOut + i * 3 * sizeof(float), &n, sizeof(vec3));
#endif
    }
}

void calculateTangents(size_t vertexCount, const float* vertex, const float* normal,
    const float* texcoord, size_t triangleCount, const unsigned int* indices, float* tangentOut)
{
    // Iterate through faces.
    for (size_t faceIndex = 0; faceIndex < triangleCount; faceIndex++)
    {
        unsigned int i0 = indices[faceIndex * 3];
        unsigned int i1 = indices[faceIndex * 3 + 1];
        unsigned int i2 = indices[faceIndex * 3 + 2];

        // Assume flattened vertices if no indices.
        if (indices)
        {
            i0 = indices[faceIndex * 3];
            i1 = indices[faceIndex * 3 + 1];
            i2 = indices[faceIndex * 3 + 2];
        }
        else
        {
            i0 = static_cast<unsigned int>(faceIndex * 3);
            i1 = static_cast<unsigned int>(faceIndex * 3 + 1);
            i2 = static_cast<unsigned int>(faceIndex * 3 + 2);
        }

        const vec3& p0 = *reinterpret_cast<const vec3*>(&vertex[i0 * 3]);
        const vec3& p1 = *reinterpret_cast<const vec3*>(&vertex[i1 * 3]);
        const vec3& p2 = *reinterpret_cast<const vec3*>(&vertex[i2 * 3]);

        const vec2& w0 = *reinterpret_cast<const vec2*>(&texcoord[i0 * 2]);
        const vec2& w1 = *reinterpret_cast<const vec2*>(&texcoord[i1 * 2]);
        const vec2& w2 = *reinterpret_cast<const vec2*>(&texcoord[i2 * 2]);

        vec3& t0 = *reinterpret_cast<vec3*>(&tangentOut[i0 * 3]);
        vec3& t1 = *reinterpret_cast<vec3*>(&tangentOut[i1 * 3]);
        vec3& t2 = *reinterpret_cast<vec3*>(&tangentOut[i2 * 3]);

        // Based on Eric Lengyel at http://www.terathon.com/code/tangent.html
        vec3 e1     = p1 - p0;
        vec3 e2     = p2 - p0;
        float x1    = w1[0] - w0[0];
        float x2    = w2[0] - w0[0];
        float y1    = w1[1] - w0[1];
        float y2    = w2[1] - w0[1];
        float denom = x1 * y2 - x2 * y1;
        float r     = (fabsf(denom) > 1.0e-12) ? (1.0f / denom) : 0.0f;
        vec3 t      = (e1 * y2 - e2 * y1) * r;
        t0 += t;
        t1 += t;
        t2 += t;
    }
    // Iterate through vertices.
    for (size_t v = 0; v < vertexCount; v++)
    {
        const vec3& n = *reinterpret_cast<const vec3*>(&normal[v * 3]);
        vec3& t       = *reinterpret_cast<vec3*>(&tangentOut[v * 3]);
        if (t != vec3(0.0f))
        {
            // Gram-Schmidt orthogonalize.
            t = normalize(t - n * dot(n, t));
        }
        else
        {
            // Generate an arbitrary tangent.
            // https://graphics.pixar.com/library/OrthonormalB/paper.pdf
            float sign = (n[2] < 0.0f) ? -1.0f : 1.0f;
            float a    = -1.0f / (sign + n[2]);
            float b    = n[0] * n[1] * a;
            t          = vec3(1.0f + sign * n[0] * n[0] * a, sign * b, -sign * n[0]);
        }
    }
}

} // namespace Foundation
} // namespace Aurora