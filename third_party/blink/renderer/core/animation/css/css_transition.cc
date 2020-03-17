// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_transition.h"

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSTransition::CSSTransition(ExecutionContext* execution_context,
                             AnimationTimeline* timeline,
                             AnimationEffect* content,
                             const PropertyHandle& transition_property)
    : Animation(execution_context, timeline, content),
      transition_property_(transition_property) {}

AtomicString CSSTransition::transitionProperty() const {
  return transition_property_.GetCSSPropertyName().ToAtomicString();
}

String CSSTransition::playState() const {
  // TODO(1043778): Flush is likely not required once the CSSTransition is
  // disassociated from its owning element.
  if (GetDocument())
    GetDocument()->UpdateStyleAndLayoutTree();
  return Animation::playState();
}

void CSSTransition::setEffect(AnimationEffect* new_effect) {
  AnimationEffect* old_effect = effect();
  Animation::setEffect(new_effect);
  if (new_effect == old_effect)
    return;

  AnimationEffect::EventDelegate* old_event_delegate =
      old_effect ? old_effect->GetEventDelegate() : nullptr;

  // When the animation no longer has an associated effect, calls to
  // Animation::Update will no longer update the animation timing and,
  // consequently, do not trigger transition events. Each transitionrun or
  // transitionstart requires a corresponding transitionend or transitioncancel.
  // https://drafts.csswg.org/css-transitions-2/#event-dispatch
  if (!new_effect && old_effect && old_event_delegate) {
    // If the transition has no target effect, the transition phase is set
    // according to the first matching condition from below:
    //   If the transition has an unresolved current time,
    //     The transition phase is ‘idle’.
    //   If the transition has a current time < 0,
    //     The transition phase is ‘before’.
    //   Otherwise,
    //     The transition phase is ‘after’.
    base::Optional<double> current_time = CurrentTimeInternal();
    Timing::Phase phase;
    if (!current_time)
      phase = Timing::kPhaseNone;
    else if (current_time < 0)
      phase = Timing::kPhaseBefore;
    else
      phase = Timing::kPhaseAfter;
    old_event_delegate->OnEventCondition(*old_effect, phase);
    return;
  }

  if (!new_effect)
    return;

  // TODO(crbug.com/1043778): Determine if changing the properties being
  // animated should reset the owning element.

  // Attach an event delegate to the new effect.
  Element* target = To<KeyframeEffect>(new_effect)->target();
  AnimationEffect::EventDelegate* new_event_delegate =
      CSSAnimations::CreateEventDelegate(target, transition_property_,
                                         old_event_delegate);
  new_effect->SetEventDelegate(new_event_delegate);

  // Force an update to the timing model to ensure correct ordering of
  // transition events.
  Update(kTimingUpdateOnDemand);
}

}  // namespace blink
