// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include <tuple>

#include "base/stl_util.h"

namespace cc {

EventMetrics::EventMetrics(ui::EventType type, base::TimeTicks time_stamp)
    : type_(type), time_stamp_(time_stamp) {}

const char* EventMetrics::GetTypeName() const {
  DCHECK(IsWhitelisted()) << "Event type is not whitelisted for event metrics: "
                          << type_;

  switch (type_) {
    case ui::ET_MOUSE_PRESSED:
      return "MousePressed";
    case ui::ET_MOUSE_RELEASED:
      return "MouseReleased";
    case ui::ET_MOUSEWHEEL:
      return "MouseWheel";
    case ui::ET_KEY_PRESSED:
      return "KeyPressed";
    case ui::ET_KEY_RELEASED:
      return "KeyReleased";
    case ui::ET_TOUCH_PRESSED:
      return "TouchPressed";
    case ui::ET_TOUCH_RELEASED:
      return "TouchReleased";
    case ui::ET_TOUCH_MOVED:
      return "TouchMoved";
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool EventMetrics::IsWhitelisted() const {
  switch (type_) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_MOVED:
      return true;
    default:
      return false;
  }
}

bool EventMetrics::operator==(const EventMetrics& other) const {
  return std::tie(type_, time_stamp_) ==
         std::tie(other.type_, other.time_stamp_);
}

}  // namespace cc
