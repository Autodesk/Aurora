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

#include "Aurora/Foundation/Log.h"

#include <Aurora/Foundation/Utilities.h>

#if _WIN32
#include <Windows.h>
#endif

namespace Aurora
{
namespace Foundation
{

// The singleton logger instance.
Log theLogger;

// Get the singleton logger instance.
Log& Log::logger()
{
    return theLogger;
}

void Log::debugBreak()
{
#if !defined(NDEBUG)
#if defined(_WIN32)
    __debugbreak();
#endif
#endif
}

bool Log::displayFailureDialog([[maybe_unused]] const std::string& file, [[maybe_unused]] int line,
    [[maybe_unused]] const std::string& msg)
{
#if defined(_WIN32)
    std::string dialogMsg = msg + "File:" + file + "\nLine:" + std::to_string(line) + "\n";

    // On windows debug builds display message as error message box
    int msgboxID = MessageBox(NULL, s2w(dialogMsg).c_str(),
        (LPCWSTR)L"Aurora Failure (click Cancel to ignore)", MB_ICONERROR | MB_OKCANCEL);

    // If message box is not canceled, then return true to trigger abort.
    return (msgboxID != IDCANCEL);
#else
    // On non-windows platforms just return true to trigger abort.
    return true;
#endif
}

void Log::writeToConsole([[maybe_unused]] const std::string& msg)
{
#if defined(_WIN32)
    // Output to windows debug console.
    std::wstring widestr = std::wstring(msg.begin(), msg.end());
    OutputDebugString(widestr.c_str());
#endif
}

} // namespace Foundation
} // namespace Aurora
