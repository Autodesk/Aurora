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

// NOTE: This file requires the use of glm, the vector math library.

#include <Aurora/Foundation/BoundingBox.h>
#include <Aurora/Foundation/Log.h>
#include <Aurora/Foundation/Plane.h>
#include <array>
#include <glm/glm.hpp>

//#define USE_SPHERICAL_BOUNDS 1

namespace Aurora
{
namespace Foundation
{

// A class representing a view frustum.
template <class PLANE, class MATRIX, class VEC3, class VEC4, class BBOX>
class FrustumT
{
public:
    //*** Lifetime Management ***

    enum Boundary
    {
        Left,
        Right,
        Bottom,
        Top,
        Near,
        Far
    };

    /// Constructors.
    FrustumT() = default;

    /// Initialize frustum planes from a matrix.
    FrustumT(const MATRIX& mat) { setFrom(mat); }

    /// Extract frustum planes from a model-view-projection matrix using the Gribb/Hartmann method.
    /// The planes are defined in the source coordinate system of the matrix.
    /// Assumes column-major matrix layout.
    /// http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
    void setFrom(const MATRIX& mat)
    {
        _planes[Left]   = row(mat, 3) + row(mat, 0);
        _planes[Right]  = row(mat, 3) - row(mat, 0);
        _planes[Bottom] = row(mat, 3) + row(mat, 1);
        _planes[Top]    = row(mat, 3) - row(mat, 1);
        _planes[Near]   = row(mat, 2); // Z is in the range of [0..1].
        _planes[Far]    = row(mat, 3) - row(mat, 2);

        for (auto& plane : _planes)
            plane.normalize();
    }

    /// Returns true if the point is inside the view frustum.
    bool contains(const VEC3& point, bool farClip = false) const
    {
        size_t plnCount = farClip ? _planes.size() : _planes.size() - 1;
        for (size_t i = 0; i < plnCount; i++)
            if (_planes[i].classify(point) == Halfspace::Negative)
                return false;

        // The point is inside the frustum.
        return true;
    }

    /// Returns true if the bounding box intersects the view frustum.
    bool intersects(const BBOX& box, bool farClip = false) const
    {
        size_t plnCount = farClip ? _planes.size() : _planes.size() - 1;
        for (size_t i = 0; i < plnCount; i++)
        {
#if defined(USE_SPHERICAL_BOUNDS)
            if (_planes[i].distanceTo(box.center()) < -box.radius())
                ;
            return false;
#else
            if (_planes[i].inLower(box))
                return false;
#endif
        }

        // The box intersects the frustum.
        return true;
    }

    /// Returns true if the bounding box is completely inside the view frustum.
    bool contains(const BBOX& box, bool farClip = false) const
    {
        size_t plnCount = farClip ? _planes.size() : _planes.size() - 1;
        for (int i = 0; i < plnCount; i++)
        {
#if defined(USE_SPHERICAL_BOUNDS)
            double d = _planes[i].distanceTo(box.center());
            if (d < 0.0 || d < box.radius())
                return false;
#else
            if (!_planes[i].inUpper(box))
                return false;
#endif
        }

        // The box is inside the frustum.
        return true;
    }

private:
    // Row element access.
    VEC4 row(const MATRIX& m, unsigned int index) const
    {
        return VEC4(m[0][index], m[1][index], m[2][index], m[3][index]);
    }

    /*** Private Variables ***/
    std::array<PLANE, 6> _planes;
};

/// A class representing a single precision view frustum.
using Frustum = FrustumT<Plane, glm::mat4, glm::vec3, glm::vec4, BoundingBox>;

/// A class representing a double precision view frustum.
using FrustumDbl = FrustumT<PlaneDbl, glm::dmat4, glm::dvec3, glm::dvec4, BoundingBoxDbl>;

} // namespace Foundation
} // namespace Aurora
