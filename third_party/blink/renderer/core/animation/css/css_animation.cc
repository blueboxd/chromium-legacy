// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSAnimation::CSSAnimation(ExecutionContext* execution_context,
                           AnimationTimeline* timeline,
                           AnimationEffect* content,
                           const String& animation_name)
    : Animation(execution_context, timeline, content),
      animation_name_(animation_name),
      sticky_play_state_(Animation::kUnset) {}

String CSSAnimation::playState() const {
  if (GetDocument())
    GetDocument()->UpdateStyleAndLayoutTree();
  return Animation::playState();
}

void CSSAnimation::pause(ExceptionState& exception_state) {
  sticky_play_state_ = kPaused;
  Animation::pause(exception_state);
}

void CSSAnimation::play(ExceptionState& exception_state) {
  sticky_play_state_ = kRunning;
  Animation::play(exception_state);
}

}  // namespace blink
