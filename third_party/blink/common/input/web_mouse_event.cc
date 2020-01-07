// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_mouse_event.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace blink {

WebMouseEvent::WebMouseEvent(WebInputEvent::Type type,
                             const WebGestureEvent& gesture_event,
                             Button button_param,
                             int click_count_param,
                             int modifiers,
                             base::TimeTicks time_stamp,
                             PointerId id_param)
    : WebInputEvent(type, modifiers, time_stamp),
      WebPointerProperties(id_param,
                           WebPointerProperties::PointerType::kMouse,
                           button_param),
      click_count(click_count_param) {
  DCHECK_GE(type, kMouseTypeFirst);
  DCHECK_LE(type, kMouseTypeLast);
  SetPositionInWidget(gesture_event.PositionInWidget());
  SetPositionInScreen(gesture_event.PositionInScreen());
  SetFrameScale(gesture_event.FrameScale());
  SetFrameTranslate(gesture_event.FrameTranslate());
  SetMenuSourceType(gesture_event.GetType());
}

gfx::PointF WebMouseEvent::PositionInRootFrame() const {
  return gfx::ScalePoint(position_in_widget_, 1 / frame_scale_) +
         frame_translate_;
}

std::unique_ptr<WebInputEvent> WebMouseEvent::Clone() const {
  return std::make_unique<WebMouseEvent>(*this);
}

WebMouseEvent WebMouseEvent::FlattenTransform() const {
  WebMouseEvent result = *this;
  result.FlattenTransformSelf();
  return result;
}

void WebMouseEvent::FlattenTransformSelf() {
  position_in_widget_ = PositionInRootFrame();
  frame_translate_ = gfx::Vector2dF();
  frame_scale_ = 1;
}

void WebMouseEvent::SetMenuSourceType(WebInputEvent::Type type) {
  switch (type) {
    case kGestureTapDown:
    case kGestureTap:
    case kGestureDoubleTap:
      menu_source_type = kMenuSourceTouch;
      break;
    case kGestureLongPress:
      menu_source_type = kMenuSourceLongPress;
      break;
    case kGestureLongTap:
      menu_source_type = kMenuSourceLongTap;
      break;
    default:
      menu_source_type = kMenuSourceNone;
  }
}

}  // namespace blink
