// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_MOUSE_WHEEL_EVENT_TRAITS_H_
#define CONTENT_COMMON_INPUT_WEB_MOUSE_WHEEL_EVENT_TRAITS_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace content {

// Utility class for performing operations on and with WebMouseWheelEvent.
class CONTENT_EXPORT WebMouseWheelEventTraits {
 public:
  // Returns the *platform specific* event action corresponding with the wheel
  // event.
  static blink::WebMouseWheelEvent::EventAction GetEventAction(
      const blink::WebMouseWheelEvent& event);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMouseWheelEventTraits);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_MOUSE_WHEEL_EVENT_TRAITS_H_
