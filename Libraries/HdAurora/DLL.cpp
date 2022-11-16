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
#include "pch.h"

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "HdAuroraPlugin.h"

#if defined WIN32
// The module entry point
BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        HdRendererPluginRegistry::Define<HdAuroraRendererPlugin>(); // register HdAurora plugin with
                                                                    // USD
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#else
extern "C"
{
    __attribute__((constructor)) static void Initializer(
        int /*argc*/, char** /*argv*/, char** /*envp*/)
    {
        HdRendererPluginRegistry::Define<HdAuroraRendererPlugin>(); // register HdAurora plugin with
                                                                    // USD
    }

    __attribute__((destructor)) static void Finalizer() {}
}
#endif