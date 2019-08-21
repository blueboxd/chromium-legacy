// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_

#include <stdint.h>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"

namespace device {

class HidWriter;

class Dualshock4Controller final : public AbstractHapticGamepad {
 public:
  Dualshock4Controller(std::unique_ptr<HidWriter> hid_writer);
  ~Dualshock4Controller() override;

  static bool IsDualshock4(uint16_t vendor_id, uint16_t product_id);

  // AbstractHapticGamepad public implementation.
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  // AbstractHapticGamepad private implementation.
  void DoShutdown() override;

  std::unique_ptr<HidWriter> writer_;
  base::WeakPtrFactory<Dualshock4Controller> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_H_
