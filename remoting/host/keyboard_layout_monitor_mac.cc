// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include "base/callback.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorMac : public KeyboardLayoutMonitor {
 public:
  KeyboardLayoutMonitorMac(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  }
  ~KeyboardLayoutMonitorMac() override = default;
  void Start() override {
    // TODO(rkjnsn): Poll keyboard layout on Mac.
  }
};

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return std::make_unique<KeyboardLayoutMonitorMac>(std::move(callback));
}

}  // namespace remoting
