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

#include "Properties.h"

BEGIN_AURORA

// Forward declarations.
class HGIScene;

// An internal implementation for ILight.
class HGILight : public ILight, public FixedValues
{
public:
    /*** Lifetime Management ***/

    HGILight(HGIScene* pScene, const string& lightType, int index);
    ~HGILight() { _pScene = nullptr; };

    /*** Functions ***/
    FixedValues& values() override { return *this; }

    int index() const { return _index; }

    bool isDirty() const { return _bIsDirty; }
    void clearDirtyFlag() { _bIsDirty = false; }

private:
    /*** Private Variables ***/

    HGIScene* _pScene = nullptr;
    int _index;
};

MAKE_AURORA_PTR(HGILight);

END_AURORA
