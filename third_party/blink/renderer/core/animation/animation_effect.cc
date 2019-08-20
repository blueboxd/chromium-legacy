/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/animation_effect.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/computed_effect_timing.h"
#include "third_party/blink/renderer/core/animation/optional_effect_timing.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"

namespace blink {

AnimationEffect::AnimationEffect(const Timing& timing,
                                 EventDelegate* event_delegate)
    : owner_(nullptr),
      timing_(timing),
      event_delegate_(event_delegate),
      calculated_(),
      needs_update_(true),
      last_update_time_(NullValue()) {
  timing_.AssertValid();
}

void AnimationEffect::UpdateSpecifiedTiming(const Timing& timing) {
  // FIXME: Test whether the timing is actually different?
  timing_ = timing;
  InvalidateAndNotifyOwner();
}

EffectTiming* AnimationEffect::getTiming() const {
  return SpecifiedTiming().ConvertToEffectTiming();
}

ComputedEffectTiming* AnimationEffect::getComputedTiming() const {
  return SpecifiedTiming().getComputedTiming(EnsureCalculated(),
                                             IsKeyframeEffect());
}

void AnimationEffect::updateTiming(OptionalEffectTiming* optional_timing,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/827178): Determine whether we should pass a Document in here
  // (and which) to resolve the CSS secure/insecure context against.
  if (!TimingInput::Update(timing_, optional_timing, nullptr, exception_state))
    return;
  InvalidateAndNotifyOwner();
}

void AnimationEffect::UpdateInheritedTime(double inherited_time,
                                          TimingUpdateReason reason) const {
  const Timing::AnimationDirection direction =
      (GetAnimation() && GetAnimation()->playbackRate() < 0)
          ? Timing::AnimationDirection::kBackwards
          : Timing::AnimationDirection::kForwards;
  bool needs_update =
      needs_update_ ||
      (last_update_time_ != inherited_time &&
       !(IsNull(last_update_time_) && IsNull(inherited_time))) ||
      (owner_ && owner_->EffectSuppressed());
  needs_update_ = false;
  last_update_time_ = inherited_time;

  const double local_time = inherited_time;
  if (needs_update) {
    Timing::CalculatedTiming calculated = SpecifiedTiming().CalculateTimings(
        local_time, direction, IsKeyframeEffect());

    const bool was_canceled = calculated.phase != calculated_.phase &&
                              calculated.phase == Timing::kPhaseNone;

    // If the animation was canceled, we need to fire the event condition before
    // updating the timing so that the cancelation time can be determined.
    if (was_canceled && event_delegate_) {
      // TODO(jortaylo): OnEventCondition uses the new phase but the old current
      // iterations. That is why we partially update *calculated_* here with the
      // new phase. This pseudo state can be very confusing. It may be either a
      // bug or required but at the very least instead of partially updating
      // *calculated_* we should pass in current phase and the old "current
      // iterations" more explicitly. https://crbug.com/994850
      calculated_.phase = calculated.phase;
      event_delegate_->OnEventCondition(*this);
    }

    calculated_ = calculated;
  }

  // Test for events even if timing didn't need an update as the animation may
  // have gained a start time.
  // FIXME: Refactor so that we can DCHECK(owner_) here, this is currently
  // required to be nullable for testing.
  if (reason == kTimingUpdateForAnimationFrame &&
      (!owner_ || owner_->IsEventDispatchAllowed())) {
    if (event_delegate_)
      event_delegate_->OnEventCondition(*this);
  }

  if (needs_update) {
    // FIXME: This probably shouldn't be recursive.
    UpdateChildrenAndEffects();
    calculated_.time_to_forwards_effect_change = CalculateTimeToEffectChange(
        true, local_time, calculated_.time_to_next_iteration);
    calculated_.time_to_reverse_effect_change = CalculateTimeToEffectChange(
        false, local_time, calculated_.time_to_next_iteration);
  }
}

void AnimationEffect::InvalidateAndNotifyOwner() const {
  Invalidate();
  if (owner_)
    owner_->EffectInvalidated();
}

const Timing::CalculatedTiming& AnimationEffect::EnsureCalculated() const {
  if (owner_)
    owner_->UpdateIfNecessary();
  return calculated_;
}

Animation* AnimationEffect::GetAnimation() {
  return owner_ ? owner_->GetAnimation() : nullptr;
}
const Animation* AnimationEffect::GetAnimation() const {
  return owner_ ? owner_->GetAnimation() : nullptr;
}

void AnimationEffect::Trace(blink::Visitor* visitor) {
  visitor->Trace(owner_);
  visitor->Trace(event_delegate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
