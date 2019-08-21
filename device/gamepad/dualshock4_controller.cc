// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller.h"

#include <array>

#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/hid_writer.h"

namespace {
const uint8_t kRumbleMagnitudeMax = 0xff;
}  // namespace

namespace device {

Dualshock4Controller::Dualshock4Controller(std::unique_ptr<HidWriter> writer)
    : writer_(std::move(writer)) {}

Dualshock4Controller::~Dualshock4Controller() = default;

// static
bool Dualshock4Controller::IsDualshock4(uint16_t vendor_id,
                                        uint16_t product_id) {
  auto gamepad_id = GamepadIdList::Get().GetGamepadId(vendor_id, product_id);
  return gamepad_id == GamepadId::kSonyProduct05c4 ||
         gamepad_id == GamepadId::kSonyProduct09cc;
}

void Dualshock4Controller::DoShutdown() {
  writer_.reset();
}

void Dualshock4Controller::SetVibration(double strong_magnitude,
                                        double weak_magnitude) {
  DCHECK(writer_);
  std::array<uint8_t, 32> control_report;
  control_report.fill(0);
  control_report[0] = 0x05;  // report ID
  control_report[1] = 0x01;  // motor only, don't update LEDs
  control_report[4] =
      static_cast<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);
  control_report[5] =
      static_cast<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);

  writer_->WriteOutputReport(control_report);
}

base::WeakPtr<AbstractHapticGamepad> Dualshock4Controller::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
