//
// Copyright 2023 by Autodesk, Inc.  All rights reserved.
//
// This computer source code and related instructions and comments
// are the unpublished confidential and proprietary information of
// Autodesk, Inc. and are protected under applicable copyright and
// trade secret law.  They may not be disclosed to, copied or used
// by any third party without the prior written consent of Autodesk, Inc.
//
#include "pch.h"

#include "MaterialDefinition.h"

BEGIN_AURORA

MaterialDefaultValues::MaterialDefaultValues(const UniformBufferDefinition& propertyDefs,
    const vector<PropertyValue>& defaultProps, const vector<TextureDefinition>& defaultTxt) :
    propertyDefinitions(propertyDefs), properties(defaultProps), textures(defaultTxt)
{
    AU_ASSERT(
        defaultProps.size() == propertyDefs.size(), "Default properties do not match definition");
    for (size_t i = 0; i < defaultTxt.size(); i++)
    {
        textureNames.push_back(defaultTxt[i].name);
    }
}

END_AURORA
