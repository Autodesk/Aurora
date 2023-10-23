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

BEGIN_AURORA

// An internal implementation for IGeometry.
class GeometryBase : public IGeometry
{
public:
    /*** Types ***/

    /*** Lifetime Management ***/

    GeometryBase(const std::string& name, const GeometryDescriptor& descriptor);
    ~GeometryBase() {};

    /*** Functions ***/

    uint32_t vertexCount() { return _vertexCount; }
    const string& name() { return _name; }

    // Is the geometry complete (has posiitons can be used as primary geometry for instance.)
    bool isIncomplete() { return _incomplete; }

    const vector<float>& positions() { return _positions; }
    const vector<float>& normals() { return _normals; }
    const vector<float>& tangents() { return _tangents; }
    const vector<float>& texCoords() { return _texCoords; }

protected:
    /*** Private Functions ***/

    /*** Private Variables ***/

    std::string _name;
    bool _bIsDirty        = true;
    uint32_t _vertexCount = 0;
    vector<float> _positions;
    vector<float> _normals;
    vector<float> _tangents;
    vector<float> _texCoords;
    uint32_t _indexCount = 0;
    vector<uint32_t> _indices;
    bool _incomplete = false;

    /*** DirectX 12 Objects ***/
};

END_AURORA
