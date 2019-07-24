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
  bool needs_update =
      needs_update_ ||
      (last_update_time_ != inherited_time &&
       !(IsNull(last_update_time_) && IsNull(inherited_time))) ||
      (owner_ && owner_->EffectSuppressed());
  needs_update_ = false;
  last_update_time_ = inherited_time;

  const double local_time = inherited_time;
  double time_to_next_iteration = std::numeric_limits<double>::infinity();
  if (needs_update) {
    const double active_duration = SpecifiedTiming().ActiveDuration();
    const AnimationDirection direction =
        (GetAnimation() && GetAnimation()->playbackRate() < 0) ? kBackwards
                                                               : kForwards;

    const Timing::Phase current_phase =
        CalculatePhase(active_duration, local_time, direction, timing_);
    const double active_time = CalculateActiveTime(
        active_duration, timing_.ResolvedFillMode(IsKeyframeEffect()),
        local_time, current_phase, timing_);

    base::Optional<double> progress;
    const double iteration_duration =
        SpecifiedTiming().IterationDuration().InSecondsF();

    const double overall_progress = CalculateOverallProgress(
        current_phase, active_time, iteration_duration, timing_.iteration_count,
        timing_.iteration_start);
    const double simple_iteration_progress = CalculateSimpleIterationProgress(
        current_phase, overall_progress, timing_.iteration_start, active_time,
        active_duration, timing_.iteration_count);
    const double current_iteration = CalculateCurrentIteration(
        current_phase, active_time, timing_.iteration_count, overall_progress,
        simple_iteration_progress);
    const bool current_direction_is_forwards =
        IsCurrentDirectionForwards(current_iteration, timing_.direction);
    const double directed_progress = CalculateDirectedProgress(
        simple_iteration_progress, current_iteration, timing_.direction);

    progress = CalculateTransformedProgress(
        current_phase, directed_progress, iteration_duration,
        current_direction_is_forwards, timing_.timing_function);
    if (IsNull(progress.value())) {
      progress.reset();
    }

    // Conditionally compute the time to next iteration, which is only
    // applicable if the iteration duration is non-zero.
    if (iteration_duration) {
      const double start_offset = MultiplyZeroAlwaysGivesZero(
          timing_.iteration_start, iteration_duration);
      DCHECK_GE(start_offset, 0);
      const double offset_active_time =
          CalculateOffsetActiveTime(active_duration, active_time, start_offset);
      const double iteration_time = CalculateIterationTime(
          iteration_duration, active_duration, offset_active_time, start_offset,
          current_phase, timing_);
      if (!IsNull(iteration_time)) {
        time_to_next_iteration = iteration_duration - iteration_time;
        if (active_duration - active_time < time_to_next_iteration)
          time_to_next_iteration = std::numeric_limits<double>::infinity();
      }
    }

    const bool was_canceled = current_phase != calculated_.phase &&
                              current_phase == Timing::kPhaseNone;
    calculated_.phase = current_phase;
    // If the animation was canceled, we need to fire the event condition before
    // updating the timing so that the cancelation time can be determined.
    if (was_canceled && event_delegate_)
      event_delegate_->OnEventCondition(*this);

    calculated_.current_iteration = current_iteration;
    calculated_.progress = progress;

    calculated_.is_in_effect = !IsNull(active_time);
    calculated_.is_in_play = GetPhase() == Timing::kPhaseActive;
    calculated_.is_current = GetPhase() == Timing::kPhaseBefore || IsInPlay();
    calculated_.local_time = last_update_time_;
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
    calculated_.time_to_forwards_effect_change =
        CalculateTimeToEffectChange(true, local_time, time_to_next_iteration);
    calculated_.time_to_reverse_effect_change =
        CalculateTimeToEffectChange(false, local_time, time_to_next_iteration);
  }
}

void AnimationEffect::InvalidateAndNotifyOwner() const {
  Invalidate();
  if (owner_)
    owner_->EffectInvalidated();
}

const Timing::CalculatedTiming& AnimationEffect::EnsureCalculated() const {
  if (!owner_)
    return calculated_;

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
