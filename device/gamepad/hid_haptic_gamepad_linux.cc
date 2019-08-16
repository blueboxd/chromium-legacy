// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_linux.h"

#include "base/posix/eintr_wrapper.h"

namespace device {

HidHapticGamepadLinux::HidHapticGamepadLinux(const base::ScopedFD& fd,
                                             const HapticReportData& data)
    : HidHapticGamepadBase(data), fd_(fd.get()) {}

HidHapticGamepadLinux::~HidHapticGamepadLinux() = default;

// static
std::unique_ptr<HidHapticGamepadLinux> HidHapticGamepadLinux::Create(
    uint16_t vendor_id,
    uint16_t product_id,
    const base::ScopedFD& fd) {
  const auto* haptic_data = GetHapticReportData(vendor_id, product_id);
  if (!haptic_data)
    return nullptr;
  return std::make_unique<HidHapticGamepadLinux>(fd, *haptic_data);
}

size_t HidHapticGamepadLinux::WriteOutputReport(
    base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  ssize_t bytes_written =
      HANDLE_EINTR(write(fd_, report.data(), report.size_bytes()));
  return bytes_written < 0 ? 0 : static_cast<size_t>(bytes_written);
}

base::WeakPtr<AbstractHapticGamepad> HidHapticGamepadLinux::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
