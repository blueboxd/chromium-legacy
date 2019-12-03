// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include "base/callback.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorLinux : public KeyboardLayoutMonitor {
 public:
  KeyboardLayoutMonitorLinux(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  }
  ~KeyboardLayoutMonitorLinux() override = default;
  void Start() override {
    // TODO(rkjnsn): Poll keyboard layout on Linux.
  }
};

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return std::make_unique<KeyboardLayoutMonitorLinux>(std::move(callback));
}

}  // namespace remoting
