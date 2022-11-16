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

/// Wrapper around the DirectX 12 device.
class PTDevice
{
public:
    /// Enumeration of device features.
    ///
    /// The features are used as flags, so "enum class" is not applied here.
    enum Features
    {
        /// Default feature set (hardware-based).
        kDefault = 0x00,

        /// Software-based rendering (such as DirectX WARP).
        ///
        /// Not for normal use, for testing and other special scenarios.
        kSoftware = 0x01,

        /// Ray tracing support.
        kRayTracing = 0x02,

        /// Low power (energy saving) device.
        kLowPower = 0x04,

        /// Feature combinations.
        LowPower_RayTracing = kLowPower | kRayTracing,
        Software_RayTracing = kSoftware | kRayTracing
    };

    /// Enumeration of device vendors.
    ///
    /// These hex values are "magic numbers" used to identify GPU vendors.
    enum class Vendor
    {
        kAMD                     = 0x1002,
        kNVIDIA                  = 0x10DE,
        kIntel                   = 0x8086,
        kARM                     = 0x13B5,
        kImaginationTechnologies = 0x1010,
        kQualcomm                = 0x5143,
        kUnknown                 = -1
    };

    /// Creates a GPU device with required features and MSAA sample count.
    static unique_ptr<PTDevice> create(PTDevice::Features features, int sampleCount = 1);

    /// Do not use the constructor, use PTDevice::create() instead.
    PTDevice(PTDevice::Features features, int sampleCount);

    /// Gets the DirectX device object.
    ID3D12Device5* device() const { return _pDevice.Get(); };

    /// Gets the DirectX factory object used to create device.
    IDXGIFactory4* factory() const { return _pFactory.Get(); }

    // Gets the vendor for the DirectX device.
    Vendor vendor() { return _vendor; }

private:
    bool initialize(Features features, int sampleCount);

    // DirectX12 device object.
    ComPtr<ID3D12Device5> _pDevice;
    // DirectX 12 factor object used to create device.
    ComPtr<IDXGIFactory4> _pFactory;
    bool _isValid = false;
    // Device name.
    string _name = "";
    // Device features supported by this device.
    Features _features = Features::kDefault;
    // Vendor (int value of enum is DXGI adapter VendorId code)
    Vendor _vendor = Vendor::kUnknown;
    // MSAA sampler count for device.
    uint32_t _sampleCount = 0;
    // MSAA sample quality for device.
    uint32_t _sampleQuality = 0;
};

END_AURORA