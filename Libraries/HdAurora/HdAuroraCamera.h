//**************************************************************************/
// Copyright (c) 2022 Autodesk, Inc.
// All rights reserved.
//
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc., and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//**************************************************************************/
// DESCRIPTION: Camera for HdAurora.
// AUTHOR: Autodesk Inc.
//**************************************************************************/

#pragma once

class HdAuroraRenderDelegate;

class HdAuroraCamera : public HdCamera
{
public:
    HdAuroraCamera(pxr::SdfPath const& sprimId, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraCamera() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(
        HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

private:
    HdAuroraRenderDelegate* _owner;
};
