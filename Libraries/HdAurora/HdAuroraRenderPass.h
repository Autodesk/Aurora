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

class HdAuroraRenderBuffer;
class HdAuroraRenderDelegate;

class HdAuroraRenderPass final : public HdRenderPass
{
public:
    HdAuroraRenderPass(HdRenderIndex* index, HdRprimCollection const& collection,
        HdAuroraRenderDelegate* renderDelegate);
    ~HdAuroraRenderPass();

    bool IsConverged() const override;

protected:
    void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
        TfTokenVector const& renderTags) override;

    void _MarkCollectionDirty() override {}

private:
    HdAuroraRenderDelegate* _owner;

    HdRenderPassAovBindingVector _aovBindings;
    std::pair<int, int> _viewportSize { 0, 0 };
    unsigned int _frameBufferTexture = 0;
    unsigned int _frameBufferObject  = 0;

    std::shared_ptr<HdAuroraRenderBuffer> _ownedRenderBuffer;
    std::map<TfToken, HdAuroraRenderBuffer*> _renderBuffers;

    GfMatrix4f _cameraView, _cameraProj;
};