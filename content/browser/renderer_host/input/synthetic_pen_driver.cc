// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pen_driver.h"

namespace content {

SyntheticPenDriver::SyntheticPenDriver() : SyntheticMouseDriver() {
  mouse_event_.pointer_type = blink::WebPointerProperties::PointerType::kPen;
}

SyntheticPenDriver::~SyntheticPenDriver() {}

void SyntheticPenDriver::Leave(int index) {
  DCHECK_EQ(index, 0);
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseLeave, mouse_event_.PositionInWidget().x(),
      mouse_event_.PositionInWidget().y(), last_modifiers_,
      mouse_event_.pointer_type);
}

}  // namespace content
