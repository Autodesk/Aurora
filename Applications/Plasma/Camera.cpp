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
#include "pch.h"

#include "Camera.h"

// The canonical direction vectors.
static constexpr vec3 kForward = vec3(0.0f, 0.0f, -1.0f);
static constexpr vec3 kRight   = vec3(1.0f, 0.0f, 0.0f);
static constexpr vec3 kUp      = vec3(0.0f, 1.0f, 0.0f);

const mat4& Camera::viewMatrix()
{
    // Create a view matrix if it needs to be updated.
    if (_isViewDirty)
    {
        _viewMatrix  = lookAt(_eye, _target, _up);
        _isViewDirty = false;
    }

    return _viewMatrix;
}

const mat4& Camera::projMatrix()
{
    // NOTE: The near / far clip distances are not relevant for ray tracing. In order to support
    // rasterization, the clip distances should be computed from the scene bounds and the current
    // camera position. See Fit() for how the view Z bounds can be determined.

    // Do nothing if the projection matrix is not dirty.
    if (!_isProjDirty)
    {
        return _projMatrix;
    }

    // Compute the proper orthographic or perspective projection matrix.
    if (_isOrtho)
    {
        // Compute the size of the view at the target distance. This ensures that orthographic and
        // perspective projections will have the same "zoom" at the target point.
        // NOTE: Any changes to the target distance must invalidate the projection matrix.
        vec2 size;
        size.y = length(_target - _eye) * tan(_fov * 0.5f);
        size.x = aspectRatio() * size.y;

        // Create a (right-hand) orthographic projection matrix with the computed view size.
        _projMatrix = orthoRH(-size.x, size.x, -size.y, size.y, _near, _far);
    }
    else
    {
        // Compute the perspective projection matrix.
        _projMatrix = perspective(_fov, aspectRatio(), _near, _far);
    }

    _isProjDirty = false;

    return _projMatrix;
}

void Camera::setIsOrtho(bool value)
{
    _isOrtho     = value;
    _isProjDirty = true;
}

void Camera::setView(const vec3& eye, const vec3& target)
{
    // Set the view properties.
    _eye    = eye;
    _target = target;
    _up     = kUp;

    // Compute a valid perpendicular up vector from the direction and (provided, rough) up vectors.
    vec3 forward = forwardDir();
    _up          = cross(rightDir(), forward);

    // Compute the azimuth and elevation angles from the forward direction.
    _azimuth   = atan2(forward.x, -forward.z);
    _elevation = asin(forward.y);

    // Set the view and projection matrices as dirty.
    // NOTE: The projection matrix is dirty because the target distance may have changed here.
    _isViewDirty = true;
    _isProjDirty = true;
}

void Camera::setProjection(float fov, float nearClip, float farClip)
{
    assert(fov > 0.0f && nearClip > 0.0f && farClip > 0.0f);

    // Set the projection properties.
    _fov  = fov;
    _near = nearClip;
    _far  = farClip;

    _isProjDirty = true;
}

void Camera::setDimensions(const ivec2& dimensions)
{
    assert(dimensions.x > 0 && dimensions.y > 0);

    // Set the dimensions.
    _dimensions = dimensions;

    _isProjDirty = true;
}

void Camera::fit(const Foundation::BoundingBox& bounds)
{
    fit(bounds, forwardDir());
}

void Camera::fit(const Foundation::BoundingBox& bounds, const vec3& direction)
{
    // Set the target to the center of the bounding box, and temporarily set an eye position an
    // arbitrary distance from the new target, using the specified direction.
    vec3 target = bounds.center();
    vec3 eye    = target - direction;

    // Transform the bounding box corners to view space, and create a new view space bounding box
    // from them. This results in a conservative box.
    setView(eye, target);
    Foundation::BoundingBox viewBox = bounds.transform(viewMatrix());

    // Compute half of the camera FOV in radians, using the vertical or horizontal FOV depending
    // on the relative aspect ratios of the camera and the camera box. Also compute the width or
    // height to cover in the view, again based on the relative aspect ratios.
    // NOTE: In GLM, the FOV value covers the *full* vertical view, but *half* is needed here for
    // trigonometric calculations.
    vec3 boxDims     = viewBox.dimensions();
    float boxAspect  = boxDims.x / boxDims.y;
    float viewAspect = aspectRatio();
    float halfFOV    = boxAspect > viewAspect ? atan(tan(_fov * 0.5f) * viewAspect) : _fov * 0.5f;
    float size       = boxAspect > viewAspect ? boxDims.x : boxDims.y;

    // Compute the distance to keep the camera away from the box and have the box fill the computed
    // FOV. This includes half the box depth (Z), since this is the distance from the target. Set
    // the eye position as the computed distance away from the target.
    float distance = size / (2.0f * tan(halfFOV)) + boxDims.z / 2.0f;
    _eye           = target - distance * direction;

    // Set the view and projection matrices as dirty.
    // NOTE: The projection matrix is dirty because the target distance may have changed here.
    _isViewDirty = true;
    _isProjDirty = true;
}

void Camera::mouseMove(int xPos, int yPos, const Inputs& inputs)
{
    // Handle wheel input immediately by dollying the camera, then returning.
    if (inputs.Wheel)
    {
        // For each wheel "delta" (1 or -1), treat that as dollying 10%.
        dolly(vec2(0.0f, -yPos / 10.0f));
        return;
    }

    // If an update is not in progress, and a mouse button is pressed, start updating. Stop further
    // processing regardless of whether a mouse button is pressed, i.e. wait for the next event.
    if (!_isUpdating)
    {
        if (inputs.LeftButton || inputs.MiddleButton || inputs.RightButton)
        {
            _isUpdating = true;
            _lastMouse  = vec2(xPos, yPos);
        }

        return;
    }

    // Get the change in mouse position, as a fraction of the viewport dimensions.
    vec2 currentMouse = vec2(xPos, yPos);
    vec2 delta        = currentMouse - _lastMouse;
    delta /= _dimensions;
    _lastMouse = currentMouse;

    // Update the camera based on the pressed mouse button and the change in mouse position. If
    // no button is pressed, stop updating.
    if (inputs.LeftButton)
    {
        orbit(delta);
    }
    else if (inputs.RightButton)
    {
        pan(delta);
    }
    else if (inputs.MiddleButton)
    {
        dolly(delta);
    }
    else
    {
        _isUpdating = false;
    }
}

void Camera::orbit(const vec2& delta)
{
    // Increment and clamp the azimuth and elevation angles from the provided delta. The azimuth
    // angle is in [0, 360] wrapped, and the elevation angle is in [-89, 89] clamped.
    constexpr float kOrbitRate     = radians(360.0f);
    constexpr float kAzimuthMax    = radians(360.0f);
    constexpr vec2 kElevationRange = vec2(radians(-89.0f), radians(89.0f));
    _azimuth -= delta.x * kOrbitRate;
    _elevation -= delta.y * kOrbitRate;
    _azimuth   = fmod(fmod(_azimuth, kAzimuthMax) + kAzimuthMax, kAzimuthMax);
    _elevation = glm::clamp(_elevation, kElevationRange.x, kElevationRange.y);

    // Create rotation matrices: rotate around the canonical right axis using the elevation, and
    // around the canonical up axis using the azimuth angle. Then apply both rotations to the
    // canonical forward vector, to create a direction vector.
    mat4 rotateAzimuth   = rotate(_azimuth, kUp);
    mat4 rotateElevation = rotate(_elevation, kRight);
    vec3 direction       = rotateAzimuth * rotateElevation * vec4(kForward, 0.0f);

    // Set the eye position at the existing distance away from the target, in the new direction.
    // Also update the up vector.
    _eye         = _target - direction * targetDistance();
    vec3 right   = cross(direction, kUp);
    _up          = cross(right, direction);
    _isViewDirty = true;
}

void Camera::pan(const vec2& delta)
{
    // Compute the dimensions of the world covered by the camera at the target distance.
    float halfTanFOVY = tan(_fov * 0.5f);
    vec2 size(halfTanFOVY * aspectRatio(), halfTanFOVY);
    size *= targetDistance() * 2.0f;

    // Compute the amount to shift the camera: the right vector scaled by the X delta (a fraction
    // of the world size) and X world size, plus the up vector scaled by the X delta and world size.
    // This way the pan exactly follows the mouse, at the target distance.
    vec3 translate = rightDir() * -delta.x * size.x + _up * delta.y * size.y;

    // Shift both the eye and target positions.
    _eye += translate;
    _target += translate;
    _isViewDirty = true;
}

void Camera::dolly(const vec2& delta)
{
    // Scale the distance between the eye and target by the change in Y, e.g. a Y value of 0.1
    // means increasing the distance by 1.1x (dolly out), and a Y value of -0.1 means scaling by
    // 0.9x (dolly in).
    float distance = targetDistance();
    distance *= 1.0f + glm::max(-0.9f, delta.y);

    // Set the eye position to the computed distance away from the target.
    _eye = _target - forwardDir() * distance;

    // Set the view and projection matrices as dirty.
    // NOTE: The projection matrix is dirty because the target distance may have changed here.
    _isViewDirty = true;
    _isProjDirty = true;
}