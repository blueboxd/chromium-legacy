// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_

#include <memory>

#include <IOKit/hid/IOHIDManager.h>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/hid_haptic_gamepad_base.h"

namespace device {

class HidHapticGamepadMac final : public HidHapticGamepadBase {
 public:
  HidHapticGamepadMac(IOHIDDeviceRef device_ref, const HapticReportData& data);
  ~HidHapticGamepadMac() override;

  static std::unique_ptr<HidHapticGamepadMac> Create(uint16_t vendor_id,
                                                     uint16_t product_id,
                                                     IOHIDDeviceRef device_ref);

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  // HidHapticGamepadBase implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  IOHIDDeviceRef device_ref_;
  base::WeakPtrFactory<HidHapticGamepadMac> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_MAC_H_
