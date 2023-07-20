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

#ifndef DISABLE_UNIT_TESTS

#include <Aurora/Foundation/BoundingBox.h>
#include <Aurora/Foundation/Frustum.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "TestHelpers.h"

using namespace std;
using namespace glm;
using namespace Aurora::Foundation;

namespace
{

class MathTest : public ::testing::Test
{
public:
    MathTest() {}
    ~MathTest() {}

    // Optionally override the base class's setUp function to create some
    // data for the tests to use.
    virtual void SetUp() override {}

    // Optionally override the base class's tearDown to clear out any changes
    // and release resources.
    virtual void TearDown() override {}
};

TEST_F(MathTest, TestBoundingBox)
{
    // Test radius.
    BoundingBox box1(vec3(-1.0f, 0.0f, 0.0f), vec3(1.0, 0.0, 0.0));
    ASSERT_FLOAT_EQ(box1.radius(), 1.7320508f);
    ASSERT_FLOAT_EQ(box1.min().x, -1.0f);
    ASSERT_FLOAT_EQ(box1.min().y, 0.0f);
    ASSERT_FLOAT_EQ(box1.min().z, 0.0f);
    ASSERT_FLOAT_EQ(box1.max().x, 1.0f);
    ASSERT_FLOAT_EQ(box1.max().y, 0.0f);
    ASSERT_FLOAT_EQ(box1.max().z, 0.0f);
    ASSERT_EQ(box1.isValid(), true);

    box1.add(vec3(0.0f, -1.0f, 1.0f));
    box1.add(vec3(0.0f, 1.0f, -1.0f));
    ASSERT_FLOAT_EQ(box1.min().y, -1.0f);
    ASSERT_FLOAT_EQ(box1.max().z, 1.0f);

    BoundingBox box2(vec3(-4.0f, 0.0f, 0.0f), vec3(-2.0, 0.0, 0.0));
    ASSERT_FLOAT_EQ(box2.radius(), 1.7320508f);

    BoundingBox box3(vec3(0.0f, -4.0f, 0.0f), vec3(0.0, -2.0, 0.0));
    ASSERT_FLOAT_EQ(box2.radius(), 1.7320508f);
}

TEST_F(MathTest, TestFrustum)
{
    BoundingBox box1(vec3(-1.0f, 0.0f, 0.0f), vec3(1.0, 0.0, 0.0));
    BoundingBox box2(vec3(-4.0f, 0.0f, 0.0f), vec3(-2.0, 0.0, 0.0));
    BoundingBox box3(vec3(0.0f, -4.0f, 0.0f), vec3(0.0, -2.0, 0.0));

    mat4 VM  = lookAtRH(vec3(0.0f, 0.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat4 PM  = orthoRH_ZO(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    mat4 VPM = PM * VM;
    Frustum frustum(VPM);

    // Test view frustum visibility.
    ASSERT_EQ(frustum.intersects(box1), true);
    ASSERT_EQ(frustum.intersects(box2), false);
    ASSERT_EQ(frustum.intersects(box3), false);
    ASSERT_EQ(frustum.contains(vec3(0.0f, 0.0f, 0.0f)), true);
    ASSERT_EQ(frustum.contains(vec3(2.0f, 0.0f, 0.0f)), false);
    ASSERT_EQ(frustum.contains(vec3(0.0f, 2.0f, 0.0f)), false);
    ASSERT_EQ(frustum.contains(vec3(-2.0f, 0.0f, 0.0f)), false);
    ASSERT_EQ(frustum.contains(vec3(0.0f, -2.0f, 0.0f)), false);
    ASSERT_EQ(frustum.contains(vec3(1.0f, 0.0f, 0.0f)), true);
    ASSERT_EQ(frustum.contains(vec3(0.0f, 1.0f, 0.0f)), true);

    // Define some random positions.
    vector<vec3> vertices = { vec3(5.0f, 7.0f, 15.0f), vec3(200.0f, 1.0, -30.0f),
        vec3(356.0f, -148.0f, 30.0f), vec3(-321.0f, -34.0f, -1.0f) };

    for (auto& v : vertices)
    {
        // Set up a camera centered on a unit sized bbox.
        BoundingBox box(v - vec3(1.0f), v + vec3(1.0f));
        vec3 position = box.center() + vec3(0.0f, 0.0f, 4.0f);
        VM            = lookAtRH(position, box.center(), vec3(0.0f, 1.0f, 0.0f));
        PM            = perspectiveRH(radians(45.0f), 1.0f, 0.1f, 100.0f);
        VPM           = PM * VM;

        // Test positives.
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), true);

        // Get camera offset distance from spherical radius.
        float d = box.radius() * 2.5f;

        // Adjust camera, pan so that the box out-of-view, left, right etc.
        position = box.center() + vec3(0.0f, 0.0f, 4.0f);
        vec3 offset(-d, 0.0f, 0.0f);
        VM  = lookAtRH(position + offset, box.center() + offset, vec3(0.0f, 1.0f, 0.0f));
        VPM = PM * VM;
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), false);

        offset = vec3(d, 0.0f, 0.0f);
        VM     = lookAtRH(position + offset, box.center() + offset, vec3(0.0f, 1.0f, 0.0f));
        VPM    = PM * VM;
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), false);

        offset = vec3(0.0f, d, 0.0f);
        VM     = lookAtRH(position + offset, box.center() + offset, vec3(0.0f, 1.0f, 0.0f));
        VPM    = PM * VM;
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), false);

        offset = vec3(0.0f, -d, 0.0f);
        VM     = lookAtRH(position + offset, box.center() + offset, vec3(0.0f, 1.0f, 0.0f));
        VPM    = PM * VM;
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), false);

        // Reduce offset, bbox should now be visible.
        offset = vec3(0.0f, d * 0.5f, 0.0f);
        VM     = lookAtRH(position + offset, box.center() + offset, vec3(0.0f, 1.0f, 0.0f));
        VPM    = PM * VM;
        frustum.setFrom(VPM);
        ASSERT_EQ(frustum.intersects(box), true);
    }
}

} // namespace

#endif
