// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller_mac.h"

#include <CoreFoundation/CoreFoundation.h>

namespace device {

Dualshock4ControllerMac::Dualshock4ControllerMac(IOHIDDeviceRef device_ref)
    : device_ref_(device_ref) {}

Dualshock4ControllerMac::~Dualshock4ControllerMac() = default;

void Dualshock4ControllerMac::DoShutdown() {
  device_ref_ = nullptr;
}

size_t Dualshock4ControllerMac::WriteOutputReport(
    base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  if (!device_ref_)
    return 0;

  IOReturn success =
      IOHIDDeviceSetReport(device_ref_, kIOHIDReportTypeOutput, report[0],
                           report.data(), report.size_bytes());
  return (success == kIOReturnSuccess) ? report.size_bytes() : 0;
}

base::WeakPtr<AbstractHapticGamepad> Dualshock4ControllerMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
