// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class CORE_EXPORT CSSTransition : public Animation {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSTransition(ExecutionContext*,
                AnimationTimeline*,
                AnimationEffect*,
                const PropertyHandle& transition_property);

  bool IsCSSTransition() const final { return true; }

  AtomicString transitionProperty() const;
  const CSSProperty& TransitionCSSProperty() const {
    return transition_property_.GetCSSProperty();
  }

  // Animation overrides.
  // Various operations may affect the computed values of properties on
  // elements. User agents may, as an optimization, defer recomputing these
  // values until it becomes necessary; however, all operations included in the
  // programming interfaces defined in the web-animations and css-transitions
  // specifications, must produce a result consistent with having fully
  // processed any such pending changes to computed values.  Notably, setting
  // display:none must update the play state.
  // https://drafts.csswg.org/css-transitions-2/#requirements-on-pending-style-changes
  String playState() const override;

  // Effects associated with a CSSTransition use an event delegate to queue
  // transition events triggered from changes to the timing phase of an
  // animation. This override ensures that an event delegate is associated with
  // the new effect, or that the transition is properly ended|canceled in the
  // case of a null effect.
  void setEffect(AnimationEffect* effect) override;

 private:
  PropertyHandle transition_property_;
};

template <>
struct DowncastTraits<CSSTransition> {
  static bool AllowFrom(const Animation& animation) {
    return animation.IsCSSTransition();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_H_
