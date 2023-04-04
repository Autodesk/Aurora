//
// Copyright 2023 by Autodesk, Inc.  All rights reserved.
//
// This computer source code and related instructions and comments
// are the unpublished confidential and proprietary information of
// Autodesk, Inc. and are protected under applicable copyright and
// trade secret law.  They may not be disclosed to, copied or used
// by any third party without the prior written consent of Autodesk, Inc.
//
#pragma once

namespace Aurora
{
namespace Foundation
{

void calculateNormals(size_t vertexCount, const float* vertex, size_t triangleCount,
    const unsigned int* indices, float* normalOut);

void calculateTangents(size_t vertexCount, const float* vertex, const float* normal,
    const float* texcoord, size_t triangleCount, const unsigned int* indices, float* tangentOut);

} // namespace Foundation
} // namespace Aurora