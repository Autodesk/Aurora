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

#include "BSDFCodeGenerator.h"

BEGIN_AURORA

struct MaterialTypeSource;

namespace MaterialXCodeGen
{

class MaterialGenerator
{
public:
    MaterialGenerator(IRenderer* pRenderer, const string& mtlxFolder);

    /// Generate shader code for material.
    bool generate(const string& document, map<string, Value>* pDefaultValuesOut,
        MaterialTypeSource& sourceOut);

    // Generate shared definition functions.
    void generateDefinitions(string& definitionHLSLOut);

    // Get the code generator used to generate material shader code.
    MaterialXCodeGen::BSDFCodeGenerator& codeGenerator() { return *_pCodeGenerator; }

private:
    // Code generator used to generate MaterialX files.
    unique_ptr<MaterialXCodeGen::BSDFCodeGenerator> _pCodeGenerator;

    // Mapping from a MaterialX output property to a Standard Surface property.
    map<string, string> _materialXOutputParamMapping;

    IRenderer* _pRenderer;
};

} // namespace MaterialXCodeGen

END_AURORA
