// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller_base.h"

#include <array>

namespace {
const uint16_t kVendorSony = 0x054c;
const uint16_t kProductDualshock4 = 0x05c4;
const uint16_t kProductDualshock4Slim = 0x9cc;
const uint8_t kRumbleMagnitudeMax = 0xff;

enum ControllerType {
  UNKNOWN_CONTROLLER,
  DUALSHOCK4_CONTROLLER,
  DUALSHOCK4_SLIM_CONTROLLER
};

ControllerType ControllerTypeFromDeviceIds(uint16_t vendor_id,
                                           uint16_t product_id) {
  if (vendor_id == kVendorSony) {
    switch (product_id) {
      case kProductDualshock4:
        return DUALSHOCK4_CONTROLLER;
      case kProductDualshock4Slim:
        return DUALSHOCK4_SLIM_CONTROLLER;
      default:
        break;
    }
  }
  return UNKNOWN_CONTROLLER;
}

}  // namespace

namespace device {

Dualshock4ControllerBase::~Dualshock4ControllerBase() = default;

// static
bool Dualshock4ControllerBase::IsDualshock4(uint16_t vendor_id,
                                            uint16_t product_id) {
  return ControllerTypeFromDeviceIds(vendor_id, product_id) !=
         UNKNOWN_CONTROLLER;
}

void Dualshock4ControllerBase::SetVibration(double strong_magnitude,
                                            double weak_magnitude) {
  std::array<uint8_t, 32> control_report;
  control_report.fill(0);
  control_report[0] = 0x05;  // report ID
  control_report[1] = 0x01;  // motor only, don't update LEDs
  control_report[4] =
      static_cast<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);
  control_report[5] =
      static_cast<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);

  WriteOutputReport(control_report);
}

}  // namespace device
