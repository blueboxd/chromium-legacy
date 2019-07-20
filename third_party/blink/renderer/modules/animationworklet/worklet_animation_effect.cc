// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_effect.h"
#include "third_party/blink/renderer/core/animation/computed_effect_timing.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"

namespace blink {

WorkletAnimationEffect::WorkletAnimationEffect(
    base::Optional<base::TimeDelta> local_time,
    const Timing& specified_timing)
    : local_time_(local_time),
      specified_timing_(specified_timing),
      calculated_() {
  specified_timing_.AssertValid();
}

EffectTiming* WorkletAnimationEffect::getTiming() const {
  return specified_timing_.ConvertToEffectTiming();
}

ComputedEffectTiming* WorkletAnimationEffect::getComputedTiming() const {
  UpdateInheritedTime(local_time_.has_value() ? local_time_.value().InSecondsF()
                                              : NullValue());
  return specified_timing_.getComputedTiming(calculated_, false);
}

// TODO(jortaylo): This function is meant to replicate similar logic found in
// AnimationEffect::UpdateInheritedTime. It has been updated here only to remove
// unneeded sections of the code that don't apply within worklet scope.
// It needs to be moved to Timing so that it can be shared between
// AnimationEffect and WorkletAnimationEffect. https://crbug.com/915344.
void WorkletAnimationEffect::UpdateInheritedTime(double inherited_time) const {
  bool needs_update = (last_update_time_ != inherited_time &&
                       !(IsNull(last_update_time_) && IsNull(inherited_time)));

  if (!needs_update)
    return;

  last_update_time_ = inherited_time;
  const double local_time = inherited_time;

  const double active_duration = specified_timing_.ActiveDuration();
  const AnimationEffect::AnimationDirection direction =
      AnimationEffect::kForwards;
  const Timing::Phase current_phase =
      CalculatePhase(active_duration, local_time, direction, specified_timing_);

  const double active_time = CalculateActiveTime(
      active_duration, specified_timing_.ResolvedFillMode(false), local_time,
      current_phase, specified_timing_);
  base::Optional<double> progress;
  const double iteration_duration =
      specified_timing_.IterationDuration().InSecondsF();
  const double overall_progress = CalculateOverallProgress(
      current_phase, active_time, iteration_duration,
      specified_timing_.iteration_count, specified_timing_.iteration_start);
  const double simple_iteration_progress = CalculateSimpleIterationProgress(
      current_phase, overall_progress, specified_timing_.iteration_start,
      active_time, active_duration, specified_timing_.iteration_count);
  const double current_iteration = CalculateCurrentIteration(
      current_phase, active_time, specified_timing_.iteration_count,
      overall_progress, simple_iteration_progress);
  const bool current_direction_is_forwards = IsCurrentDirectionForwards(
      current_iteration, specified_timing_.direction);
  const double directed_progress =
      CalculateDirectedProgress(simple_iteration_progress, current_iteration,
                                specified_timing_.direction);

  progress = CalculateTransformedProgress(
      current_phase, directed_progress, iteration_duration,
      current_direction_is_forwards, specified_timing_.timing_function);
  if (IsNull(progress.value())) {
    progress.reset();
  }

  if (iteration_duration) {
    calculated_.phase = current_phase;
    calculated_.current_iteration = current_iteration;
    calculated_.progress = progress;
    calculated_.is_in_effect = !IsNull(active_time);
    calculated_.is_in_play = calculated_.phase == Timing::kPhaseActive;
    calculated_.is_current =
        calculated_.phase == Timing::kPhaseBefore || calculated_.is_in_play;
    calculated_.local_time = last_update_time_;
  }

  return;
}

void WorkletAnimationEffect::setLocalTime(double time_ms, bool is_null) {
  if (is_null) {
    local_time_.reset();
    return;
  }
  DCHECK(!std::isnan(time_ms));
  // Convert double to base::TimeDelta because cc/animation expects
  // base::TimeDelta.
  //
  // Note on precision loss: base::TimeDelta has microseconds precision which is
  // also the precision recommended by the web animation specification as well
  // [1]. If the input time value has a bigger precision then the conversion
  // causes precision loss. Doing the conversion here ensures that reading the
  // value back provides the actual value we use in further computation which
  // is the least surprising path.
  // [1] https://drafts.csswg.org/web-animations/#precision-of-time-values
  local_time_ = base::TimeDelta::FromMillisecondsD(time_ms);
}

double WorkletAnimationEffect::localTime(bool& is_null) const {
  is_null = !local_time_.has_value();
  return local_time_.value_or(base::TimeDelta()).InMillisecondsF();
}

base::Optional<base::TimeDelta> WorkletAnimationEffect::local_time() const {
  return local_time_;
}
}  // namespace blink
