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

#include "HdAuroraPlugin.h"
#include "HdAuroraRenderDelegate.h"

// Always dump logs to VS debug console.
// TODO: Turned on for now for development purposes, we should turn off and use ASDK product tools
// for internal logs.
#define AU_DEV_ALWAYS_DUMP_LOGS 1

void setLogFunction()
{
    auto logCB = [](const std::string& file, int line, Aurora::Foundation::Log::Level level,
                     const std::string& msg) {
        std::string fullMsg = file.empty() ? "" : (file + ", " + std::to_string(line) + ": ");
        fullMsg += msg;

        if (AU_DEV_ALWAYS_DUMP_LOGS)
            Aurora::Foundation::Log::writeToConsole(fullMsg);

        switch (level)
        {
        case Aurora::Foundation::Log::Level::kInfo:
            TF_STATUS(fullMsg);
            break;
        case Aurora::Foundation::Log::Level::kWarn:
            TF_WARN(fullMsg);
            break;
        case Aurora::Foundation::Log::Level::kError:
            TF_RUNTIME_ERROR(fullMsg);
            break;
        case Aurora::Foundation::Log::Level::kFail:
            TF_FATAL_ERROR(fullMsg);
            break;
        case Aurora::Foundation::Log::Level::kNone:
            break;
        }

        return false;
    };

    Aurora::logger().setLogFunction(logCB);
    Aurora::Foundation::Log::logger().setLogFunction(logCB);
}

HdRenderDelegate* HdAuroraRendererPlugin::CreateRenderDelegate()
{
    setLogFunction();

    return new HdAuroraRenderDelegate();
}

HdRenderDelegate* HdAuroraRendererPlugin::CreateRenderDelegate(
    HdRenderSettingsMap const& settingsMap)
{
    setLogFunction();

    return new HdAuroraRenderDelegate(settingsMap);
}

void HdAuroraRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate)
{
    delete renderDelegate;
}

bool HdAuroraRendererPlugin::IsSupported() const
{
    return true;
}
