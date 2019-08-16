// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_MAC_H_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_MAC_H_

#include <IOKit/hid/IOHIDManager.h>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/dualshock4_controller_base.h"

namespace device {

class Dualshock4ControllerMac final : public Dualshock4ControllerBase {
 public:
  Dualshock4ControllerMac(IOHIDDeviceRef device_ref);
  ~Dualshock4ControllerMac() override;

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  // Dualshock4ControllerBase implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  IOHIDDeviceRef device_ref_;
  base::WeakPtrFactory<Dualshock4ControllerMac> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_MAC_H_
