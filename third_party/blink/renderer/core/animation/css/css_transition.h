// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT CSSTransition : public Animation {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSTransition* Create(AnimationEffect*,
                               AnimationTimeline*,
                               const PropertyHandle&);

  CSSTransition(ExecutionContext*,
                AnimationTimeline*,
                AnimationEffect*,
                const PropertyHandle& transition_property);

  AtomicString transitionProperty() const;

 private:
  PropertyHandle transition_property_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_
