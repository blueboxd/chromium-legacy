// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pointer_lock_detector.h"

#include "remoting/proto/event.pb.h"

namespace remoting {
PointerLockDetector::PointerLockDetector(InputStub* input_stub,
                                         EventHandler* event_handler)
    : InputFilter(input_stub), event_handler_(event_handler) {}

PointerLockDetector::~PointerLockDetector() = default;

void PointerLockDetector::InjectMouseEvent(const protocol::MouseEvent& event) {
  bool is_active = event.has_delta_x() || event.has_delta_y();
  if (is_active != is_active_ || !has_triggered_) {
    event_handler_->OnPointerLockChanged(is_active);
    is_active_ = is_active;
    has_triggered_ = true;
  }

  InputFilter::InjectMouseEvent(event);
}

}  // namespace remoting
