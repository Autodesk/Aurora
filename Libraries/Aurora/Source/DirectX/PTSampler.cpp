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

#include "PTSampler.h"

#include "PTRenderer.h"

BEGIN_AURORA

PTSampler::PTSampler(PTRenderer* /* pRenderer*/, const Properties& props)
{
    // Initailize with default values.
    _desc                = D3D12_SAMPLER_DESC();
    _desc.Filter         = D3D12_FILTER_ANISOTROPIC;
    _desc.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    _desc.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    _desc.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    _desc.MipLODBias     = 0.0f;
    _desc.MaxAnisotropy  = 16;
    _desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    _desc.BorderColor[0] = 1.0f;
    _desc.BorderColor[1] = 1.0f;
    _desc.BorderColor[2] = 1.0f;
    _desc.BorderColor[3] = 1.0f;
    _desc.MinLOD         = 0.0f;
    _desc.MaxLOD         = D3D12_FLOAT32_MAX;

    // Set the U address mode from properties.
    if (props.find(Names::SamplerProperties::kAddressModeU) != props.end())
        _desc.AddressU = valueToDXAddressMode(props.at(Names::SamplerProperties::kAddressModeU));

    // Set the V address mode from properties.
    if (props.find(Names::SamplerProperties::kAddressModeV) != props.end())
        _desc.AddressV = valueToDXAddressMode(props.at(Names::SamplerProperties::kAddressModeV));

    // Compute hash for sampler (only consider address modes for now.)
    uint32_t addressModes[] = { (uint32_t)_desc.AddressU, (uint32_t)_desc.AddressV };
    _hash = Foundation::hashInts((uint32_t*)&addressModes, sizeof(addressModes) / sizeof(uint32_t));
}

void PTSampler::createDescriptor(
    const PTRenderer& renderer, PTSampler* pSampler, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
    // Create DX descriptor from sample description struct.
    renderer.dxDevice()->CreateSampler(pSampler->desc(), handle);
}

D3D12_TEXTURE_ADDRESS_MODE PTSampler::valueToDXAddressMode(const PropertyValue& value)
{

    // Convert property string to DX addres mode.
    string valStr = value.asString();
    if (valStr.compare(Names::AddressModes::kWrap) == 0)
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    if (valStr.compare(Names::AddressModes::kMirror) == 0)
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    if (valStr.compare(Names::AddressModes::kClamp) == 0)
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    if (valStr.compare(Names::AddressModes::kMirrorOnce) == 0)
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    if (valStr.compare(Names::AddressModes::kBorder) == 0)
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;

    // Fail if address mode not found.
    AU_FAIL("Unknown address mode:%s", value.asString().c_str());
    return (D3D12_TEXTURE_ADDRESS_MODE)-1;
}

END_AURORA
