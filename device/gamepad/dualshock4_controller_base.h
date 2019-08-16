// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_

#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class Dualshock4ControllerBase : public AbstractHapticGamepad {
 public:
  Dualshock4ControllerBase() = default;
  ~Dualshock4ControllerBase() override;

  static bool IsDualshock4(uint16_t vendor_id, uint16_t product_id);

  // AbstractHapticGamepad implementation.
  void SetVibration(double strong_magnitude, double weak_magnitude) override;

  // Sends an output report to the gamepad. Derived classes should override this
  // method with a platform-specific implementation.
  virtual size_t WriteOutputReport(base::span<const uint8_t> report) = 0;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_BASE_
