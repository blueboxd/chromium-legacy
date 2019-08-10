// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_mac.h"

#include <CoreFoundation/CoreFoundation.h>

namespace device {

HidHapticGamepadMac::HidHapticGamepadMac(IOHIDDeviceRef device_ref,
                                         const HapticReportData& data)
    : HidHapticGamepadBase(data), device_ref_(device_ref) {}

HidHapticGamepadMac::~HidHapticGamepadMac() = default;

void HidHapticGamepadMac::DoShutdown() {
  device_ref_ = nullptr;
}

// static
std::unique_ptr<HidHapticGamepadMac> HidHapticGamepadMac::Create(
    uint16_t vendor_id,
    uint16_t product_id,
    IOHIDDeviceRef device_ref) {
  const auto* haptic_data = GetHapticReportData(vendor_id, product_id);
  if (!haptic_data)
    return nullptr;
  return std::make_unique<HidHapticGamepadMac>(device_ref, *haptic_data);
}

size_t HidHapticGamepadMac::WriteOutputReport(void* report,
                                              size_t report_length) {
  if (!device_ref_)
    return 0;

  const unsigned char* report_data = static_cast<unsigned char*>(report);
  IOReturn success =
      IOHIDDeviceSetReport(device_ref_, kIOHIDReportTypeOutput, report_data[0],
                           report_data, report_length);
  return (success == kIOReturnSuccess) ? report_length : 0;
}

base::WeakPtr<AbstractHapticGamepad> HidHapticGamepadMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
