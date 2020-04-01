// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class CORE_EXPORT CSSAnimation : public Animation {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSAnimation(ExecutionContext*,
               AnimationTimeline*,
               AnimationEffect*,
               const String& animation_name);

  bool IsCSSAnimation() const final { return true; }

  void ClearOwningElement() final { owning_element_ = nullptr; }
  Element* OwningElement() const override { return owning_element_; }

  const String& animationName() const { return animation_name_; }

  // Animation overrides.
  // Various operations may affect the computed values of properties on
  // elements. User agents may, as an optimization, defer recomputing these
  // values until it becomes necessary; however, all operations included in the
  // programming interfaces defined in the web-animations and css-animations
  // specifications, must produce a result consistent with having fully
  // processed any such pending changes to computed values.  Notably, changes
  // to animation-play-state and display:none must update the play state.
  // https://drafts.csswg.org/css-animations-2/#requirements-on-pending-style-changes
  String playState() const override;
  bool pending() const override;

  // Explicit calls to the web-animation API that update the play state are
  // conditionally sticky and override the animation-play-state style.
  void pause(ExceptionState& = ASSERT_NO_EXCEPTION) override;
  void play(ExceptionState& = ASSERT_NO_EXCEPTION) override;
  void reverse(ExceptionState& = ASSERT_NO_EXCEPTION) override;
  void setStartTime(base::Optional<double>, ExceptionState&) override;

  // When set, subsequent changes to animation-play-state no longer affect the
  // play state.
  // https://drafts.csswg.org/css-animations-2/#interaction-between-animation-play-state-and-web-animations-API
  bool getIgnoreCSSPlayState() { return ignore_css_play_state_; }
  void resetIgnoreCSSPlayState() { ignore_css_play_state_ = false; }
  void Trace(blink::Visitor* visitor) override {
    Animation::Trace(visitor);
    visitor->Trace(owning_element_);
  }

  // Force pending animation properties to be applied, as these may alter the
  // animation. This step is required before any web animation API calls that
  // depends on computed values.
  void FlushPendingUpdates() const override { FlushStyles(); }

 protected:
  AnimationEffect::EventDelegate* CreateEventDelegate(
      Element* target,
      const AnimationEffect::EventDelegate* old_event_delegate) override;

 private:
  void FlushStyles() const;

  class PlayStateTransitionScope {
    STACK_ALLOCATED();

   public:
    explicit PlayStateTransitionScope(CSSAnimation& animation);
    ~PlayStateTransitionScope();

   private:
    CSSAnimation& animation_;
    bool was_paused_;
  };

  AtomicString animation_name_;

  // When set, the web-animation API is overruling the animation-play-state
  // style.
  bool ignore_css_play_state_;
  // The owning element of an animation refers to the element or pseudo-element
  // whose animation-name property was applied that generated the animation
  // The spec: https://drafts.csswg.org/css-animations-2/#owning-element-section
  Member<Element> owning_element_;
};

template <>
struct DowncastTraits<CSSAnimation> {
  static bool AllowFrom(const Animation& animation) {
    return animation.IsCSSAnimation();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_H_
