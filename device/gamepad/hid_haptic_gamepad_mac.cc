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

size_t HidHapticGamepadMac::WriteOutputReport(
    base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  if (!device_ref_)
    return 0;

  IOReturn success =
      IOHIDDeviceSetReport(device_ref_, kIOHIDReportTypeOutput, report[0],
                           report.data(), report.size_bytes());
  return (success == kIOReturnSuccess) ? report.size_bytes() : 0;
}

base::WeakPtr<AbstractHapticGamepad> HidHapticGamepadMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
