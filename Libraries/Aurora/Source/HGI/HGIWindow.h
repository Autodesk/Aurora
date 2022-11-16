// Copyright 2022 Autodesk, Inc.
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

// Forward declarations.
class HGIRenderer;

// An internal implementation for IWindow.
class HGIWindow : public IWindow
{
public:
    // Constructor and destructor.
    HGIWindow(HGIRenderer* pRenderer, WindowHandle window, uint32_t width, uint32_t height);
    ~HGIWindow() {}

    void resize(uint32_t width, uint32_t height) override;

    void setVSyncEnabled(bool /*enabled*/) override {}
};

END_AURORA