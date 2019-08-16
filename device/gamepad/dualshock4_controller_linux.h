// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_LINUX_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_LINUX_

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "device/gamepad/dualshock4_controller_base.h"

namespace device {

class Dualshock4ControllerLinux final : public Dualshock4ControllerBase {
 public:
  Dualshock4ControllerLinux(const base::ScopedFD& fd);
  ~Dualshock4ControllerLinux() override;

  // AbstractHapticGamepad implementation.
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  // Dualshock4ControllerBase implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  int fd_;  // Not owned.
  base::WeakPtrFactory<Dualshock4ControllerLinux> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_LINUX_
