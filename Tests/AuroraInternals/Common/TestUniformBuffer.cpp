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

#if !defined(DISABLE_UNIT_TESTS)

#include "TestHelpers.h"
#include <gtest/gtest.h>

// Include the Aurora PCH (this is an internal test so needs all the internal Aurora includes)
#include "pch.h"

#include "MaterialBase.h"
#include "UniformBuffer.h"

namespace
{

// Test fixture accessing test asset data path.
class UniformBufferTest : public ::testing::Test
{
public:
    UniformBufferTest() : _dataPath(TestHelpers::kSourceRoot + "/Tests/Assets") {}
    ~UniformBufferTest() {}
    const std::string& dataPath() { return _dataPath; }

protected:
    std::string _dataPath;
};

// Basic uniform buffer test, will also auto-generate the default material shaders.
TEST_F(UniformBufferTest, BasicTest)
{
    // Create a default material uniform buffer object.
    UniformBuffer defaultUniformBuffer(
        MaterialBase::StandardSurfaceUniforms, MaterialBase::StandardSurfaceDefaults.properties);

    // String for comparison error message (empty if no error.)
    std::string errMsg;

    // Comment prefix in generated text file.
    std::string commentPrefixString = "// Auto-generated by unit test " +
        std::string(testing::UnitTest::GetInstance()->current_test_info()->name()) +
        " in AuroraExternals test suite.\n\n";

    // Generate uniform buffer HLSL from uniform buffer object.
    std::string hlslString = commentPrefixString +
        defaultUniformBuffer.generateHLSLStructAndAccessors("MaterialConstants", "Material_");

    // Compare HLSL with file on disk.  Fail if differences found.
    // NOTE: This file is reused as the autogenerated shader file
    // Libraries\Aurora\Source\Shaders\DefaultMaterialUniformBuffer.slang, if changes are made to
    // default material properties this test file should be copied to
    // Libraries\Aurora\Source\Shaders.
    errMsg =
        TestHelpers::compareTextFile(dataPath() + "/TextFiles/DefaultMaterialUniformBuffer.slang",
            hlslString, "HLSL uniform buffer baseline comparison failed. ");
    ASSERT_TRUE(errMsg.empty()) << errMsg;

    // Compare HLSL with file on disk.  Fail if differences found.
    // NOTE: This file is reused as the autogenerated shader file
    // Libraries\Aurora\Source\Shaders\DefaultMaterialUniformAccessors.slang, if changes are made to
    // default material properties this test file should be copied to
    // Libraries\Aurora\Source\Shaders.
    std::string hlslAccessorString =
        commentPrefixString + defaultUniformBuffer.generateByteAddressBufferAccessors("Material_");
    errMsg = TestHelpers::compareTextFile(
        dataPath() + "/TextFiles/DefaultMaterialUniformAccessors.slang", hlslAccessorString,
        "HLSL byte address accessor baseline comparison failed. ");
    ASSERT_TRUE(errMsg.empty()) << errMsg;
}

} // namespace

#endif
