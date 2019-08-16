// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_

#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"

namespace device {

class HidHapticGamepadLinux final : public HidHapticGamepadBase {
 public:
  HidHapticGamepadLinux(const base::ScopedFD& fd, const HapticReportData& data);
  ~HidHapticGamepadLinux() override;

  static std::unique_ptr<HidHapticGamepadLinux>
  Create(uint16_t vendor_id, uint16_t product_id, const base::ScopedFD& fd);

  // AbstractHapticGamepad implementation.
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  // HidHapticGamepadBase implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  // Not owned.
  int fd_;

  base::WeakPtrFactory<HidHapticGamepadLinux> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_
