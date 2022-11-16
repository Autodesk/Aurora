#include "pch.h"

#include "HdAuroraCamera.h"
#include "HdAuroraRenderDelegate.h"
#include <pxr/base/gf/camera.h>

#include <stdexcept>

#pragma warning(disable : 4506) // inline function warning (from USD but appears in this file)

HdAuroraCamera::HdAuroraCamera(SdfPath const& rprimId, HdAuroraRenderDelegate* renderDelegate) :
    HdCamera(rprimId), _owner(renderDelegate)
{
}

HdAuroraCamera::~HdAuroraCamera()
{
    // Ensure progressive rendering is reset.
    _owner->SetSampleRestartNeeded(true);
}

HdDirtyBits HdAuroraCamera::GetInitialDirtyBitsMask() const
{
    return HdCamera::DirtyParams;
}

void HdAuroraCamera::Sync(
    HdSceneDelegate* delegate, HdRenderParam* /* renderParam */, HdDirtyBits* dirtyBits)
{

    const auto& id = GetId();
    // Laking of viewmatrix and projection matrix in HDCameraTokens
    // Camera' viewMatrix transmit directly by transform cache
    GfMatrix4f viewMat = GfMatrix4f(delegate->GetTransform(id).GetInverse());
    GfMatrix4f projMat = GfMatrix4f(0.0f);

    // Set Camera properties to aurora by GfCamera physical propeties
    float focalLength =
        (delegate->GetCameraParamValue(id, HdCameraTokens->focalLength)).Get<float>() /
        float(GfCamera::FOCAL_LENGTH_UNIT);
    float horizontalAperture =
        (delegate->GetCameraParamValue(id, HdCameraTokens->horizontalAperture)).Get<float>() /
        float(GfCamera::APERTURE_UNIT);
    float verticalAperture =
        (delegate->GetCameraParamValue(id, HdCameraTokens->verticalAperture)).Get<float>() /
        float(GfCamera::APERTURE_UNIT);
    float horizontalApertureOffset =
        (delegate->GetCameraParamValue(id, HdCameraTokens->horizontalApertureOffset)).Get<float>() /
        float(GfCamera::APERTURE_UNIT);
    float verticalApertureOffset =
        (delegate->GetCameraParamValue(id, HdCameraTokens->verticalApertureOffset)).Get<float>() /
        float(GfCamera::APERTURE_UNIT);
    GfRange1f clippingRange =
        (delegate->GetCameraParamValue(id, HdCameraTokens->clippingRange)).Get<GfRange1f>();
    HdCamera::Projection projectionType =
        (delegate->GetCameraParamValue(id, HdCameraTokens->projection)).Get<HdCamera::Projection>();

    // Set projection matrix based on projection type.
    if (HdCamera::Projection::Perspective == projectionType)
    {
        projMat[0][0] = 2 * focalLength / horizontalAperture;
        projMat[1][1] = 2 * focalLength / verticalAperture;
        projMat[2][0] = 2 * horizontalApertureOffset / horizontalAperture;
        projMat[2][1] = 2 * verticalApertureOffset / verticalAperture;
        projMat[3][2] = 2 * clippingRange.GetMin() * clippingRange.GetMax() /
            (clippingRange.GetMin() - clippingRange.GetMax());
        projMat[2][2] = (clippingRange.GetMin() + clippingRange.GetMax()) /
            (clippingRange.GetMin() - clippingRange.GetMax());
    }
    else if (HdCamera::Projection::Orthographic == projectionType)
    {
        projMat.SetIdentity();
        projMat[0][0] = (2.0f / float(GfCamera::APERTURE_UNIT)) / horizontalAperture;
        projMat[1][1] = (2.0f / float(GfCamera::APERTURE_UNIT)) / verticalAperture;
        projMat[3][0] =
            static_cast<float>(horizontalApertureOffset / (-0.5 * (horizontalAperture)));
        projMat[3][1] = static_cast<float>(verticalApertureOffset / (-0.5 * (verticalAperture)));
        projMat[2][2] = static_cast<float>(2 / (clippingRange.GetMin() + clippingRange.GetMax()));
        projMat[3][2] = (clippingRange.GetMin() + clippingRange.GetMax()) /
            (clippingRange.GetMin() - clippingRange.GetMax());
    }
    // Check to see if we need to flip the output image
    static TfToken tokenFlipYOutput("HdAuroraFlipYOutput");
    bool flipY      = true;
    auto flipYValue = this->_owner->GetRenderSetting(tokenFlipYOutput);
    if (!flipYValue.IsEmpty() && flipYValue.IsHolding<bool>())
        flipY = flipYValue.Get<bool>();
    if (flipY)
    {
        // Invert the second row of the projection matrix to flip the rendered image,
        // because OpenGL expects the first pixel to be at the *bottom* corner.
        projMat[1][0] = -projMat[1][0];
        projMat[1][1] = -projMat[1][1];
        projMat[1][2] = -projMat[1][2];
        projMat[1][3] = -projMat[1][3];
    }
    // check for camera movements
    static GfMatrix4f viewMatOld;
    static GfMatrix4f projMatOld;
    if (viewMatOld != viewMat || projMatOld != projMat)
    {
        _owner->SetSampleRestartNeeded(true);
        viewMatOld = viewMat;
        projMatOld = projMat;
    }
    _owner->GetRenderer()->setCamera((float*)&viewMat, (float*)&projMat);
    *dirtyBits = Clean;
}