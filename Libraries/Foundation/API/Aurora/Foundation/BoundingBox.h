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

#include <Aurora/Foundation/Log.h>
#include <glm/glm.hpp>

namespace Aurora
{
namespace Foundation
{

// A class template representing an axis-aligned bounding box (AABB) with a min point and max point.
template <class VEC3, class VEC4, class MATRIX, class T>
class BoundingBoxT
{
public:
    //*** Lifetime Management ***

    /// Constructor.
    BoundingBoxT() = default;

    /// Constructor with min / max points.
    BoundingBoxT(const VEC3& min, const VEC3& max) : _min(min), _max(max) {}

    //*** Functions ***

    /// Gets the minimum point of the bounding box.
    const VEC3& min() const { return _min; }

    /// Gets the maximum point of the bounding box.
    const VEC3& max() const { return _max; }

    /// Gets whether the bounding box is valid.
    bool isValid() const { return _min.x <= _max.x && _min.y <= _max.y && _min.z <= _max.z; }

    /// Gets whether the bounding box is a proper 3D volume..
    bool isVolume() const { return _min.x < _max.x && _min.y < _max.y && _min.z < _max.z; }

    /// Resets the bounding box to its initial state.
    void reset()
    {
        _min = VEC3(INFINITY);
        _max = VEC3(-INFINITY);
    }

    /// Adds the specified bounds to the bounding box.
    void add(const BoundingBoxT& bounds)
    {
        add(bounds.min());
        add(bounds.max());
    }

    /// Adds the specified pointer to the bounding box, expanding it if needed.
    void add(const VEC3& vec) { add(vec.x, vec.y, vec.z); }

    /// Adds a point with the specified coordinates to the bounding box, expanding it if needed.
    void add(T x, T y, T z)
    {
        _min.x = glm::min(x, _min.x);
        _min.y = glm::min(y, _min.y);
        _min.z = glm::min(z, _min.z);
        _max.x = glm::max(x, _max.x);
        _max.y = glm::max(y, _max.y);
        _max.z = glm::max(z, _max.z);
    }

    /// Adds an array of positions (XYZ) to the bounding box, expanding it if needed.
    void add(const T* pPositions, uint32_t positionCount = 1)
    {
        AU_ASSERT(pPositions && positionCount > 0, "Invalid arguments for BoundingBox::Add().");

        for (uint32_t i = 0; i < positionCount; i++)
        {
            add(pPositions[0], pPositions[1], pPositions[2]);
            pPositions += 3;
        }
    }

    /// Gets the center point of the bounding box.
    VEC3 center() const { return _min + dimensions() * T(0.5); }

    /// Gets the dimensions of the bounding box.
    VEC3 dimensions() const { return _max - _min; }

    /// Gets the spherical radius.
    T radius() const
    {
        auto d    = dimensions() * T(0.5);
        auto emax = std::max(std::max(d.x, d.y), d.z);
        return length(VEC3(emax, emax, emax));
    }

    /// Gets a bounding box corner with the specified index (0 - 7).
    ///
    /// \note Index 0 is the min point, and index 7 is the max point.
    VEC3 getCorner(uint32_t index) const
    {
        VEC3 result;
        result.x = (index & 1) == 0 ? _min.x : _max.x;
        result.y = (index & 2) == 0 ? _min.y : _max.y;
        result.z = (index & 4) == 0 ? _min.z : _max.z;

        return result;
    }

    /// Transforms the corners of the bounding box with the specified matrix and adds them to a new
    /// bounding box.
    BoundingBoxT transform(const MATRIX& transform, bool pdiv = false) const
    {
        BoundingBoxT result;
        for (int i = 0; i < 8; i++)
        {
            VEC4 hp = transform * VEC4(getCorner(i), 1.0);
            result.add(pdiv ? (VEC3(hp) / hp.w) : VEC3(hp));
        }

        return result;
    }

private:
    /*** Private Variables ***/

    VEC3 _min = VEC3(INFINITY);
    VEC3 _max = VEC3(-INFINITY);
};

/// A class representing a single precision bounding box.
using BoundingBox = BoundingBoxT<glm::vec3, glm::vec4, glm::mat4, float>;

/// A class representing a double precision bounding box.
using BoundingBoxDbl = BoundingBoxT<glm::dvec3, glm::dvec4, glm::dmat4, double>;

} // namespace Foundation
} // namespace Aurora
