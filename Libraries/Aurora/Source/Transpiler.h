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

#include "Properties.h"
namespace slang
{
struct IGlobalSession;
}
BEGIN_AURORA

struct AuroraSlangFileSystem;

class Transpiler
{
public:
    // Target language enum.
    enum Language
    {
        HLSL,
        GLSL
    };

    // Ctot take a map of file text strings.
    Transpiler(const std::map<std::string, const std::string&>& fileText);
    ~Transpiler();

    // Transpile a file with given name (looked up from map of file text strings)
    bool transpile(const string& shaderName, string& codeOut, string& errorOut, Language target);

    // Transpile a string containing shader code.
    bool transpileCode(
        const string& shaderCode, string& codeOut, string& errorOut, Language target);

    // Set a source file in the file text string map.
    void setSource(const string& name, const string& code);

private:
    unique_ptr<AuroraSlangFileSystem> _pFileSystem;
    slang::IGlobalSession* _pSession;
};

END_AURORA
