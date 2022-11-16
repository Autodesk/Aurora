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
class PTRenderer;

// An internal implementation for ISampler.
class PTSampler : public ISampler
{
public:
    /*** Lifetime Management ***/

    PTSampler(PTRenderer* pRenderer, const Properties& props);
    ~PTSampler() {};

    const D3D12_SAMPLER_DESC* desc() { return &_desc; }

    static void createDescriptor(
        const PTRenderer& renderer, PTSampler* pSampler, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

    size_t hash() { return _hash; }

private:
    static D3D12_TEXTURE_ADDRESS_MODE valueToDXAddressMode(const PropertyValue& value);

    D3D12_SAMPLER_DESC _desc;
    size_t _hash;
};

MAKE_AURORA_PTR(PTSampler);

END_AURORA
