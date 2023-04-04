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

class HdAuroraRenderDelegate;

class HdAuroraDomeLight : public HdLight
{
public:
    HdAuroraDomeLight(SdfPath const& sprimId, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraDomeLight() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(
        HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

private:
    HdAuroraRenderDelegate* _owner;
    std::string _environmentImageFilePath;
    Aurora::Path _auroraImagePath;
    glm::vec3 _bottomColor = glm::vec3(0, 0, 0);
    glm::vec3 _topColor    = glm::vec3(1, 1, 1);
    glm::mat4 _envTransform;
};

class HdAuroraDistantLight : public HdLight
{
public:
    HdAuroraDistantLight(SdfPath const& sprimId, HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraDistantLight() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;
    void Sync(
        HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

private:
    HdAuroraRenderDelegate* _owner;
};