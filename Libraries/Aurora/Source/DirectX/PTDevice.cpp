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
#include "pch.h"

#include "PTDevice.h"

BEGIN_AURORA

// If true then DirectX debug layer is enabled (if available.)
// Will only be available if the Graphics Tools are installed via "Apps->Optional Features" in
// Windows Settings.
// https://learn.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features
#define AU_DEVICE_DEBUG_ENABLED defined(_DEBUG) && !defined(NODXDEBUG)

static bool checkDeviceFeatures(ID3D12Device5* pDevice, PTDevice::Features features)
{
    // Check for low-power support.
    // NOTE: We check for unified memory architecture (UMA) as a proxy for low-power devices. UMA is
    // usually only supported by integrated GPUs, which are usually low-power devices.
    if (features & PTDevice::Features::kLowPower)
    {
        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture = {};
        checkHR(pDevice->CheckFeatureSupport(
            D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture)));
        if (architecture.UMA == FALSE)
        {
            return false;
        }
    }

    // Check for ray tracing support.
    if (features & PTDevice::Features::kRayTracing)
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options = { 0, D3D12_RENDER_PASS_TIER_0,
            D3D12_RAYTRACING_TIER_NOT_SUPPORTED };
        checkHR(
            pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)));
        if (options.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
        {
            return false;
        }
    }

    return true;
}

unique_ptr<Aurora::PTDevice> PTDevice::create(PTDevice::Features features, int sampleCount)
{
    // Create a device object. If it is not valid, return null.
    unique_ptr<PTDevice> pDevice = make_unique<PTDevice>(features, sampleCount);
    if (!pDevice->_isValid)
    {
        return nullptr;
    }

    return pDevice;
}

PTDevice::PTDevice(PTDevice::Features features, int sampleCount)
{
    // Initialize the device. If there is no system device that supports all of the desired
    // features, it is considered invalid.
    _isValid = initialize(features, sampleCount);
}

bool PTDevice::initialize(PTDevice::Features features, int sampleCount)
{
    // Temporary values while the factory and device are being created.
    ComPtr<IDXGIFactory4> pFactory;
    ComPtr<ID3D12Device5> pDevice;
    UINT dxgiFactoryFlags = 0;

    // If a software device is request, no other features are allowed.
    bool requireSoftware = features & PTDevice::Features::kSoftware;
    if (requireSoftware && features != PTDevice::Features::kSoftware)
    {
        return false;
    }

    // Enable the use of the debug layer on debug builds, and when NODXDEBUG is not defined.
    // NOTE: The debug layer requires installation of Graphics Tools for Windows 10, which may not
    // be desired by some clients.
#if AU_DEVICE_DEBUG_ENABLED
    // Enable the D3D12 debug layer.
    // D3D12GetDebugInterface return a null pointer if the Graphics Tools are not installed.
    ComPtr<ID3D12Debug> pDebugInterface;
    checkHR(::D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugInterface)));
    if (pDebugInterface)
    {
        // Enable the debug layer.
        pDebugInterface->EnableDebugLayer();

        // Break on DXGI errors.
        ComPtr<IDXGIInfoQueue> pDXGIInfoQueue;
        checkHR(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIInfoQueue)));
        checkHR(pDXGIInfoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
        checkHR(pDXGIInfoQueue->SetBreakOnSeverity(
            DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));

        // Create the factory with the debug flag.
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // Create the DXGI factory.
    checkHR(::CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory)));

    // Enumerate the available adapters and create a device from the first valid adapter.
    ComPtr<IDXGIAdapter1> pAdapter;
    for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; i++)
    {
        // Get the adapter information.
        DXGI_ADAPTER_DESC1 adapterDesc;
        pAdapter->GetDesc1(&adapterDesc);
        bool isSoftwareAdapter = adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;

        // If this is a software adapter, skip it if a software device *is not* required. Otherwise
        // skip it if a software device *is* required.
        if (isSoftwareAdapter)
        {
            if (!requireSoftware)
                continue;
        }
        else
        {
            if (requireSoftware)
                continue;
        }

        // Create a device from the adapter.
        checkHR(
            ::D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice)));

        // Stop searching if a valid device is found. Otherwise clear the device.
        if (!checkDeviceFeatures(pDevice.Get(), features))
        {
            pDevice.Reset();
            continue;
        }

        // Enable breaking on DirectX errors on debug builds, and when NODXDEBUG is not defined.
#if AU_DEVICE_DEBUG_ENABLED
        ComPtr<ID3D12InfoQueue> pD3D12InfoQueue;
        pDevice.As(&pD3D12InfoQueue);
        if (pD3D12InfoQueue)
        {
            checkHR(pD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
            checkHR(pD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
        }
#endif

        // Record the factory, device, and name for the selected adapter.
        _pFactory = pFactory;
        _pDevice  = pDevice;
        _name     = Foundation::w2s(adapterDesc.Description);

        // Cast the DXGI vendor ID to a vendor enumerator.
        _vendor = static_cast<Vendor>(adapterDesc.VendorId);

        // Record the feature flags.
        _features = features;

        // Check for MSAA support, determining the highest number of samples per pixel (at or below)
        // the desired count), and the associated highest sample quality level.
        for (_sampleCount = sampleCount; _sampleCount > 1; _sampleCount--)
        {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels;
            levels.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
            levels.SampleCount = _sampleCount;
            checkHR(_pDevice->CheckFeatureSupport(
                D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels)));

            // If there is at least one quality level for the sample count, record it and stop.
            if (levels.NumQualityLevels > 0)
            {
                _sampleQuality = levels.NumQualityLevels - 1;
                break;
            }
        }

        return true;
    }

    // If this point is reached, then no suitable device could be created.
    return false;
}

END_AURORA
