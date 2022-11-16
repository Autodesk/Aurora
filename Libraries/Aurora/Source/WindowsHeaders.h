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

// Windows headers.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Include DirectX-specific headers and defines.
#if defined(AU_DIRECTX)

// DirectX 12 headers.
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

// Microsoft Active Template library
#include <atlbase.h>

// DirectX 12 smart pointers.
// NOTE: ComPtr is recommended. See https://github.com/Microsoft/DirectXTK/wiki/ComPtr.
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#define MAKE_DX_PTR(_x) using _x##Ptr = ComPtr<_x>
MAKE_DX_PTR(ID3D12CommandAllocator);
MAKE_DX_PTR(ID3D12CommandQueue);
MAKE_DX_PTR(ID3D12Debug);
MAKE_DX_PTR(ID3D12DescriptorHeap);
MAKE_DX_PTR(ID3D12Device5);
MAKE_DX_PTR(ID3D12Fence);
MAKE_DX_PTR(ID3D12GraphicsCommandList4);
MAKE_DX_PTR(ID3D12InfoQueue);
MAKE_DX_PTR(ID3D12PipelineState);
MAKE_DX_PTR(ID3D12Resource);
MAKE_DX_PTR(ID3D12RootSignature);
MAKE_DX_PTR(ID3D12StateObject);
MAKE_DX_PTR(ID3D12StateObjectProperties);
MAKE_DX_PTR(ID3DBlob);
MAKE_DX_PTR(IDXGIAdapter1);
MAKE_DX_PTR(IDXGIDebug1);
MAKE_DX_PTR(IDXGIFactory4);
MAKE_DX_PTR(IDXGIInfoQueue);
MAKE_DX_PTR(IDXGISwapChain1);
MAKE_DX_PTR(IDXGISwapChain3);

// Short names for relevant DirectX 12 constants.
// NOTE: A shader record must be aligned on the byte boundary of the size specified here. Within the
// shader record, the arguments must likewise be aligned by their respective size, e.g. a root
// descriptor (or descriptor table handle) must be aligned on an 8-byte boundary.
static const size_t SHADER_ID_SIZE                = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
static const size_t SHADER_RECORD_ALIGNMENT       = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
static const size_t SHADER_RECORD_CONSTANT_SIZE   = 4; // defined by DirectX 12
static const size_t SHADER_RECORD_DESCRIPTOR_SIZE = 8; // defined by DirectX 12

#if defined(ENABLE_DENOISER)
// NVIDIA Real-time Denoisers (NRD) and NRI libraries.
#include <nvidia-nrd/Include/NRD.h>
#include <nvidia-nrd/Include/NRDDescs.h>
#include <nvidia-nri/Include/NRI.h>

// NVIDIA NRI helper interface.
// NOTE: Must be included after NRI, but before NRD integration.
#include <nvidia-nri/Include/Extensions/NRIDeviceCreation.h>
#include <nvidia-nri/Include/Extensions/NRIHelper.h>
#include <nvidia-nri/Include/Extensions/NRIWrapperD3D12.h>

// NVIDIA NRD integration utility.
// NOTE: NRI must be included before this.
#include <nvidia-nrd/Integration/NRDIntegration.h>
#endif

#endif

// Converts an HRESULT to a string for error reporting.
inline std::string hresultToString(HRESULT hr)
{
    char str[64] = {};
    ::sprintf_s(str, "HRESULT of 0x%08X", static_cast<uint32_t>(hr));

    return std::string(str);
}

// An exception for failed HRESULT values.
class HRException : public std::runtime_error
{
public:
    HRException(HRESULT hr) : std::runtime_error(::hresultToString(hr)), _hr(hr) {}
    HRESULT error() const { return _hr; }

private:
    const HRESULT _hr;
};

// Checks an HRESULT value and throws an exception if a failed HRESULT is provided.
inline void checkHR(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HRException(hr);
    }
}
