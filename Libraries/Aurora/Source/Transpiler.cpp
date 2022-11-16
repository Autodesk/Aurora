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

#include "Transpiler.h"
#include <slang.h>

BEGIN_AURORA

// Slang UUID compare operator, needed by casting operations.
bool operator==(const SlangUUID& aIn, const SlangUUID& bIn)
{
    // Use the largest type the honors the alignment of Guid
    typedef uint32_t CmpType;
    union GuidCompare
    {
        SlangUUID guid;
        CmpType data[sizeof(SlangUUID) / sizeof(CmpType)];
    };
    // Type pun - so compiler can 'see' the pun and not break aliasing rules
    const CmpType* a = reinterpret_cast<const GuidCompare&>(aIn).data;
    const CmpType* b = reinterpret_cast<const GuidCompare&>(bIn).data;
    // Make the guid comparison a single branch, by not using short circuit
    return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2]) | (a[3] ^ b[3])) == 0;
}

// Slang string blob.  Simple wrapper around std::string that can be used by Slang compiler.
struct StringSlangBlob : public ISlangBlob, ISlangCastable
{
    StringSlangBlob(const string& str) : _str(str) {}
    virtual ~StringSlangBlob() = default;

    // Get a pointer to the string.
    virtual SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() override
    {
        return _str.data();
    }

    // Get the length of the string.
    virtual SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override { return _str.length(); }

    // Default queryInterface implementation for Slang type system.
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(
        SlangUUID const& uuid, void** outObject) SLANG_OVERRIDE
    {
        *outObject = getInterface(uuid);

        return *outObject ? SLANG_OK : SLANG_E_NOT_IMPLEMENTED;
    }

    // Do not implement ref counting, just return 1.
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL addRef() SLANG_OVERRIDE { return 1; }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL release() SLANG_OVERRIDE { return 1; }

    // Default castAs implementation for Slang type system.
    void* castAs(const SlangUUID& guid) override { return getInterface(guid); }

    // Allow casting as Unknown and Blob, but nothing else.
    void* getInterface(const SlangUUID& uuid)
    {
        if (uuid == ISlangUnknown::getTypeGuid() || uuid == ISlangBlob::getTypeGuid())
        {
            return static_cast<ISlangBlob*>(this);
        }

        return nullptr;
    }

    // The actual string data.
    const string& _str;
};

// Slang filesystem that reads from a simple lookup table of strings.
struct AuroraSlangFileSystem : public ISlangFileSystem
{
    AuroraSlangFileSystem(const std::map<std::string, const std::string&>& fileText) :
        _fileText(fileText)
    {
    }
    virtual ~AuroraSlangFileSystem() = default;

    virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(
        char const* path, ISlangBlob** outBlob) override
    {
        // Is if we already have blob for this path, return that.
        auto iter = _fileBlobs.find(path);
        if (iter != _fileBlobs.end())
        {
            *outBlob = iter->second.get();

            return SLANG_OK;
        }

        // Read from the text file map.
        auto shaderIter = _fileText.find(path);
        if (shaderIter != _fileText.end())
        {
            // Create a blob from the text file string, add to the blob map, and return it.
            _fileBlobs[path] = make_unique<StringSlangBlob>(shaderIter->second);
            *outBlob         = _fileBlobs[path].get();
            return SLANG_OK;
        }

        return SLANG_FAIL;
    }

    // Slang type interface not needed, just return null.
    virtual SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(
        SlangUUID const& /* uuid*/, void** /* outObject*/) SLANG_OVERRIDE
    {
        return SLANG_E_NOT_IMPLEMENTED;
    }

    // Do not implement ref counting, just return 1.
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL addRef() SLANG_OVERRIDE { return 1; }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL release() SLANG_OVERRIDE { return 1; }

    // Slang type interface not needed, just return null.
    void* castAs(const SlangUUID&) override { return nullptr; }

    // Set a string directly as blob in file blobs map.
    void setSource(const string& name, const string& code)
    {
        _fileBlobs[name] = make_unique<StringSlangBlob>(code);
    }

    // Map of string blobs.
    map<string, unique_ptr<StringSlangBlob>> _fileBlobs;

    // Map of file strings.
    const std::map<std::string, const std::string&>& _fileText;
};

Transpiler::Transpiler(const std::map<std::string, const std::string&>& fileText)
{
    _pFileSystem = make_unique<AuroraSlangFileSystem>(fileText);
    _pSession    = spCreateSession();
}

Transpiler::~Transpiler()
{
    _pSession->release();
}

void Transpiler::setSource(const string& name, const string& code)
{
    _pFileSystem->setSource(name, code);
}

bool Transpiler::transpileCode(
    const string& shaderCode, string& codeOut, string& errorOut, Language target)
{
    // Dummy file name to use as container for shader code.
    const string codeFileName = "__shaderCode";

    // Set the shader code "file".
    setSource(codeFileName, shaderCode);

    // Transpile the shader code.
    bool res = transpile(codeFileName, codeOut, errorOut, target);

    // Clear the shader source to release memory.
    setSource(codeFileName, "");

    return res;
}

bool Transpiler::transpile(
    const string& shaderName, string& codeOut, string& errorOut, Language target)
{
    // Clear result.
    errorOut.clear();
    codeOut.clear();

    // Create a Slang compile request for transpilation.
    slang::ICompileRequest* pRequest;
    [[maybe_unused]] const int reqIndex = _pSession->createCompileRequest(&pRequest);

    // Set the file system and compile fiags.
    pRequest->setFileSystem(_pFileSystem.get());
    pRequest->setCompileFlags(SLANG_COMPILE_FLAG_NO_MANGLING);

    // Create code gen target (with GLSL or HLSL language as required).
    const int targetIndex =
        pRequest->addCodeGenTarget(target == Language::GLSL ? SLANG_GLSL : SLANG_HLSL);

    // Set target flags to generate whole program.
    pRequest->setTargetFlags(targetIndex, SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM);
    // TODO: The buffer layout might be an issue, need to work out correct flags.
    // pRequest->setTargetForceGLSLScalarBufferLayout(targetIndex, true);

    // At add translation unit from Slang to target language.
    const int translationUnitIndex =
        pRequest->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, nullptr);

    // Use standard line directives (with filename)
    pRequest->setTargetLineDirectiveMode(targetIndex, SLANG_LINE_DIRECTIVE_MODE_STANDARD);
    // Use column major matrix format.
    pRequest->setTargetMatrixLayoutMode(targetIndex, SLANG_MATRIX_LAYOUT_COLUMN_MAJOR);

    // Set shader name as source file name (file system will look up it up from file text map).
    pRequest->addTranslationUnitSourceFile(translationUnitIndex, shaderName.c_str());

    // Set DIRECTX preprocessor directive.
    pRequest->addPreprocessorDefine("DIRECTX", target == Language::GLSL ? "0" : "1");

    // Transpile the file.
    const SlangResult compileRes = pRequest->compile();
    if (compileRes != SLANG_OK)
    {
        errorOut = pRequest->getDiagnosticOutput();
        return false;
    }

    // Get blob for result.
    ISlangBlob* pOutBlob = nullptr;
    pRequest->getTargetCodeBlob(targetIndex, &pOutBlob);
    codeOut = (const char*)pOutBlob->getBufferPointer();

    // Release request.
    pRequest->release();

    return true;
}

END_AURORA
