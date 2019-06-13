// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_blocklist.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>

namespace device {
namespace {

constexpr uint16_t kVendorBlue = 0xb58e;
constexpr uint16_t kVendorLenovo = 0x17ef;
constexpr uint16_t kVendorMicrosoft = 0x045e;
constexpr uint16_t kVendorOculus = 0x2833;
constexpr uint16_t kVendorSynaptics = 0x06cb;
constexpr uint16_t kVendorWacom = 0x056a;

constexpr struct VendorProductPair {
  uint16_t vendor;
  uint16_t product;
} kBlockedDevices[] = {
    // LiteOn Lenovo Traditional USB Keyboard.
    {kVendorLenovo, 0x6099},
    // The Surface Pro 2017's detachable keyboard is a composite device with
    // several HID sub-devices. Filter out the keyboard's device ID to avoid
    // treating these sub-devices as gamepads.
    {kVendorMicrosoft, 0x0922},
    // The Lenovo X1 Yoga's Synaptics touchpad is recognized as a HID gamepad.
    {kVendorSynaptics, 0x000f},
    // The Lenovo X1 Yoga's Wacom touchscreen is recognized as a HID gamepad.
    {kVendorWacom, 0x50b8},
};

// Devices from these vendors are always blocked.
constexpr uint16_t kBlockedVendors[] = {
    // Some Blue Yeti microphones are recognized as gamepads.
    kVendorBlue,
    // Block all Oculus devices. Oculus VR controllers are handled by WebXR.
    kVendorOculus,
};

}  // namespace

bool GamepadIsExcluded(uint16_t vendor_id, uint16_t product_id) {
  const uint16_t* vendors_begin = std::begin(kBlockedVendors);
  const uint16_t* vendors_end = std::end(kBlockedVendors);
  if (std::find(vendors_begin, vendors_end, vendor_id) != vendors_end)
    return true;

  const VendorProductPair* devices_begin = std::begin(kBlockedDevices);
  const VendorProductPair* devices_end = std::end(kBlockedDevices);
  return std::find_if(
             devices_begin, devices_end, [=](const VendorProductPair& item) {
               return vendor_id == item.vendor && product_id == item.product;
             }) != devices_end;
}

}  // namespace device
