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

#ifdef WIN32

// Reports currently live DirectX objects.
static void dxReportLiveObjects()
{
#if defined AU_DIRECTX
    IDXGIDebug1Ptr pDebug;
    checkHR(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)));
    pDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
        DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
#endif
}

// The module entry point.
BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // Report memory leaks.
        dxReportLiveObjects();
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
    }

    __attribute__((destructor)) static void Finalizer() {}
}
#endif