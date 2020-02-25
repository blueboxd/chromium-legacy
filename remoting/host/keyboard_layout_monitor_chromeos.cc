// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include "base/callback.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorChromeOs : public KeyboardLayoutMonitor {
 public:
  explicit KeyboardLayoutMonitorChromeOs(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  }
  ~KeyboardLayoutMonitorChromeOs() override = default;
  void Start() override { NOTIMPLEMENTED(); }
};

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  return std::make_unique<KeyboardLayoutMonitorChromeOs>(std::move(callback));
}

}  // namespace remoting
