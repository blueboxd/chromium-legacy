// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_touch_event.h"

namespace blink {

std::unique_ptr<WebInputEvent> WebTouchEvent::Clone() const {
  return std::make_unique<WebTouchEvent>(*this);
}

WebTouchEvent WebTouchEvent::FlattenTransform() const {
  WebTouchEvent transformed_event = *this;
  for (unsigned i = 0; i < touches_length; ++i) {
    transformed_event.touches[i] = TouchPointInRootFrame(i);
  }
  transformed_event.frame_translate_ = gfx::Vector2dF();
  transformed_event.frame_scale_ = 1;

  return transformed_event;
}

WebTouchPoint WebTouchEvent::TouchPointInRootFrame(unsigned point) const {
  DCHECK_LT(point, touches_length);
  if (point >= touches_length)
    return WebTouchPoint();

  WebTouchPoint transformed_point = touches[point];
  transformed_point.radius_x /= frame_scale_;
  transformed_point.radius_y /= frame_scale_;
  transformed_point.SetPositionInWidget(
      gfx::ScalePoint(transformed_point.PositionInWidget(), 1 / frame_scale_) +
      frame_translate_);
  return transformed_point;
}

}  // namespace blink
