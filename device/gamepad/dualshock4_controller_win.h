// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_WIN_
#define DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_WIN_

#include "base/memory/weak_ptr.h"
#include "base/win/scoped_handle.h"
#include "device/gamepad/dualshock4_controller_base.h"

namespace device {

class Dualshock4ControllerWin final : public Dualshock4ControllerBase {
 public:
  explicit Dualshock4ControllerWin(HANDLE device_handle);
  ~Dualshock4ControllerWin() override;

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  // Dualshock4ControllerBase implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  base::win::ScopedHandle hid_handle_;
  base::WeakPtrFactory<Dualshock4ControllerWin> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_DUALSHOCK4_CONTROLLER_WIN_
