// Copyright 2022 Autodesk, Inc.
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
#include <glm/glm.hpp>

namespace Aurora
{
namespace Foundation
{

enum class Halfspace : int
{
    Negative = -1,
    OnPlane  = 0,
    Positive = 1
};

// A class template representing a plane.
template <class VEC3, class VEC4, class BBOX, class T>
class PlaneT
{
public:
    //*** Lifetime Management ***

    /// Constructors.
    PlaneT() = default;

    /// Define plane from equation coefficients.
    PlaneT(const VEC4& coeff) : _planeq(coeff) { normalize(); }

    /// Define plane from three points.
    PlaneT(const VEC3& A, const VEC3& B, const VEC3& C)
    {
        // clang-format off
        auto a    = (B.y - A.y)*(C.z - A.z) - (C.y - A.y)*(B.z - A.z);
        auto b    = (B.z - A.z)*(C.x - A.x) - (C.z - A.z)*(B.x - A.x);
        auto c    = (B.x - A.x)*(C.y - A.y) - (C.x - A.x)*(B.y - A.y);
        auto d    = -(a * A.x + b * A.y + c * A.z);
        // clang-format on
        _planeq = VEC4(a, b, c, d);
        normalize();
    }

    T& operator[](unsigned int index) { return _planeq[index]; }

    /// Return the signed distance to the point.
    T distanceTo(const VEC3& point) const { return glm::dot(VEC3(_planeq), point) + _planeq.w; }

    /// Classify the point in the halfspace.
    Halfspace classify(const VEC3& point) const
    {
        T d = distanceTo(point);
        if (d < 0)
            return Halfspace::Negative;
        if (d > 0)
            return Halfspace::Positive;
        return Halfspace::OnPlane;
    }

    // Returns true if the bounding box is entirely in the negative (lower) half-space.
    bool inLower(const BBOX& box) const
    {
        for (int corner = 0; corner < 8; corner++)
        {
            if (classify(box.getCorner(corner)) != Halfspace::Negative)
                return false;
        }

        // All corners are in the negative (lower) half-space.
        return true;
    }

    // Returns true if the bounding box is entirely in the positive (upper) half-space.
    bool inUpper(const BBOX& box) const
    {
        for (int corner = 0; corner < 8; corner++)
        {
            if (classify(box.getCorner(corner)) == Halfspace::Negative)
                return false;
        }

        // All corners are in the positive (upper) half-space.
        return true;
    }

    void normalize()
    {
        T mag = sqrt(glm::dot(VEC3(_planeq), VEC3(_planeq)));
        _planeq /= mag;
    }

private:
    /*** Private Variables ***/
    VEC4 _planeq;
};

/// A class representing a single precision plane.
using Plane = PlaneT<glm::vec3, glm::vec4, BoundingBox, float>;

/// A class representing a double precision plane.
using PlaneDbl = PlaneT<glm::dvec3, glm::dvec4, BoundingBoxDbl, double>;

} // namespace Foundation
} // namespace Aurora
