// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller_linux.h"

#include "base/posix/eintr_wrapper.h"

namespace device {

Dualshock4ControllerLinux::Dualshock4ControllerLinux(const base::ScopedFD& fd)
    : fd_(fd.get()) {}

Dualshock4ControllerLinux::~Dualshock4ControllerLinux() = default;

size_t Dualshock4ControllerLinux::WriteOutputReport(
    base::span<const uint8_t> report) {
  DCHECK_GE(report.size_bytes(), 1U);
  ssize_t bytes_written =
      HANDLE_EINTR(write(fd_, report.data(), report.size_bytes()));
  return bytes_written < 0 ? 0 : static_cast<size_t>(bytes_written);
}

base::WeakPtr<AbstractHapticGamepad> Dualshock4ControllerLinux::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
