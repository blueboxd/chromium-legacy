// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "cc/animation/timing_function.h"
#include "cc/base/time_util.h"
#include "ui/gfx/animation/tween.h"

const double kConstantDuration = 9.0;
const double kDurationDivisor = 60.0;

// 3 seconds limit for long-distance programmatic scrolls
const double kDeltaBasedMaxDuration = 180.0;

const double kInverseDeltaRampStartPx = 120.0;
const double kInverseDeltaRampEndPx = 480.0;
const double kInverseDeltaMinDuration = 6.0;
const double kInverseDeltaMaxDuration = 12.0;

const double kInverseDeltaSlope =
    (kInverseDeltaMinDuration - kInverseDeltaMaxDuration) /
    (kInverseDeltaRampEndPx - kInverseDeltaRampStartPx);

const double kInverseDeltaOffset =
    kInverseDeltaMaxDuration - kInverseDeltaRampStartPx * kInverseDeltaSlope;

namespace cc {

namespace {

const double kEpsilon = 0.01f;

static float MaximumDimension(const gfx::Vector2dF& delta) {
  return std::abs(delta.x()) > std::abs(delta.y()) ? delta.x() : delta.y();
}

static std::unique_ptr<TimingFunction> EaseInOutWithInitialVelocity(
    double velocity) {
  // Clamp velocity to a sane value.
  velocity = base::ClampToRange(velocity, -1000.0, 1000.0);

  // Based on CubicBezierTimingFunction::EaseType::EASE_IN_OUT preset
  // with first control point scaled.
  const double x1 = 0.42;
  const double y1 = velocity * x1;
  return CubicBezierTimingFunction::Create(x1, y1, 0.58, 1);
}

}  // namespace

base::Optional<double>
    ScrollOffsetAnimationCurve::animation_duration_for_testing_;

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    const gfx::ScrollOffset& target_value,
    std::unique_ptr<TimingFunction> timing_function,
    AnimationType animation_type,
    base::Optional<DurationBehavior> duration_behavior)
    : target_value_(target_value),
      timing_function_(std::move(timing_function)),
      animation_type_(animation_type),
      duration_behavior_(duration_behavior),
      has_set_initial_value_(false) {
  DCHECK_EQ((animation_type == AnimationType::kEaseInOut),
            duration_behavior.has_value());
}

ScrollOffsetAnimationCurve::~ScrollOffsetAnimationCurve() = default;

// static
base::TimeDelta ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
    const gfx::Vector2dF& delta,
    DurationBehavior duration_behavior,
    base::TimeDelta delayed_by) {
  double duration = kConstantDuration;
  if (!animation_duration_for_testing_) {
    switch (duration_behavior) {
      case DurationBehavior::CONSTANT:
        duration = kConstantDuration;
        break;
      case DurationBehavior::DELTA_BASED:
        duration =
            std::min<double>(std::sqrt(std::abs(MaximumDimension(delta))),
                             kDeltaBasedMaxDuration);
        break;
      case DurationBehavior::INVERSE_DELTA:
        duration = kInverseDeltaOffset +
                   std::abs(MaximumDimension(delta)) * kInverseDeltaSlope;
        duration = base::ClampToRange(duration, kInverseDeltaMinDuration,
                                      kInverseDeltaMaxDuration);
        break;
    }
    duration /= kDurationDivisor;
  } else {
    duration = animation_duration_for_testing_.value();
  }

  base::TimeDelta delay_adjusted_duration =
      base::TimeDelta::FromSecondsD(duration) - delayed_by;
  return (delay_adjusted_duration >= base::TimeDelta())
             ? delay_adjusted_duration
             : base::TimeDelta();
}

// static
base::TimeDelta ScrollOffsetAnimationCurve::LinearSegmentDuration(
    const gfx::Vector2dF& delta,
    base::TimeDelta delayed_by,
    float velocity) {
  double duration_in_seconds =
      (animation_duration_for_testing_.has_value())
          ? animation_duration_for_testing_.value()
          : std::abs(MaximumDimension(delta) / velocity);
  base::TimeDelta delay_adjusted_duration =
      base::TimeDelta::FromSecondsD(duration_in_seconds) - delayed_by;
  return (delay_adjusted_duration >= base::TimeDelta())
             ? delay_adjusted_duration
             : base::TimeDelta();
}

void ScrollOffsetAnimationCurve::SetInitialValue(
    const gfx::ScrollOffset& initial_value,
    base::TimeDelta delayed_by,
    float velocity) {
  initial_value_ = initial_value;
  has_set_initial_value_ = true;
  gfx::Vector2dF delta = target_value_.DeltaFrom(initial_value);
  switch (animation_type_) {
    case AnimationType::kEaseInOut:
      total_animation_duration_ = EaseInOutSegmentDuration(
          delta, duration_behavior_.value(), delayed_by);
      break;
    case AnimationType::kLinear:
      total_animation_duration_ =
          LinearSegmentDuration(delta, delayed_by, velocity);
      break;
  }
}

bool ScrollOffsetAnimationCurve::HasSetInitialValue() const {
  return has_set_initial_value_;
}

void ScrollOffsetAnimationCurve::ApplyAdjustment(
    const gfx::Vector2dF& adjustment) {
  initial_value_ = ScrollOffsetWithDelta(initial_value_, adjustment);
  target_value_ = ScrollOffsetWithDelta(target_value_, adjustment);
}

gfx::ScrollOffset ScrollOffsetAnimationCurve::GetValue(
    base::TimeDelta t) const {
  base::TimeDelta duration = total_animation_duration_ - last_retarget_;
  t -= last_retarget_;

  if (duration.is_zero())
    return target_value_;

  if (t <= base::TimeDelta())
    return initial_value_;

  if (t >= duration)
    return target_value_;

  double progress = timing_function_->GetValue(TimeUtil::Divide(t, duration));
  return gfx::ScrollOffset(
      gfx::Tween::FloatValueBetween(progress, initial_value_.x(),
                                    target_value_.x()),
      gfx::Tween::FloatValueBetween(progress, initial_value_.y(),
                                    target_value_.y()));
}

base::TimeDelta ScrollOffsetAnimationCurve::Duration() const {
  return total_animation_duration_;
}

AnimationCurve::CurveType ScrollOffsetAnimationCurve::Type() const {
  return SCROLL_OFFSET;
}

std::unique_ptr<AnimationCurve> ScrollOffsetAnimationCurve::Clone() const {
  return CloneToScrollOffsetAnimationCurve();
}

std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurve::CloneToScrollOffsetAnimationCurve() const {
  std::unique_ptr<TimingFunction> timing_function(
      static_cast<TimingFunction*>(timing_function_->Clone().release()));
  std::unique_ptr<ScrollOffsetAnimationCurve> curve_clone = base::WrapUnique(
      new ScrollOffsetAnimationCurve(target_value_, std::move(timing_function),
                                     animation_type_, duration_behavior_));
  curve_clone->initial_value_ = initial_value_;
  curve_clone->total_animation_duration_ = total_animation_duration_;
  curve_clone->last_retarget_ = last_retarget_;
  curve_clone->has_set_initial_value_ = has_set_initial_value_;
  return curve_clone;
}

void ScrollOffsetAnimationCurve::SetAnimationDurationForTesting(
    base::TimeDelta duration) {
  animation_duration_for_testing_ = duration.InSecondsF();
}

static base::TimeDelta VelocityBasedDurationBound(
    gfx::Vector2dF old_delta,
    double old_normalized_velocity,
    base::TimeDelta old_duration,
    gfx::Vector2dF new_delta) {
  double old_delta_max_dimension = MaximumDimension(old_delta);
  double new_delta_max_dimension = MaximumDimension(new_delta);

  // If we are already at the target, stop animating.
  if (std::abs(new_delta_max_dimension) < kEpsilon)
    return base::TimeDelta();

  // Guard against division by zero.
  if (std::abs(old_delta_max_dimension) < kEpsilon ||
      std::abs(old_normalized_velocity) < kEpsilon) {
    return base::TimeDelta::Max();
  }

  // Estimate how long it will take to reach the new target at our present
  // velocity, with some fudge factor to account for the "ease out".
  double old_true_velocity = old_normalized_velocity * old_delta_max_dimension /
                             old_duration.InSecondsF();
  double bound = (new_delta_max_dimension / old_true_velocity) * 2.5f;

  // If bound < 0 we are moving in the opposite direction.
  return bound < 0 ? base::TimeDelta::Max()
                   : base::TimeDelta::FromSecondsD(bound);
}

void ScrollOffsetAnimationCurve::UpdateTarget(
    base::TimeDelta t,
    const gfx::ScrollOffset& new_target) {
  DCHECK_NE(animation_type_, AnimationType::kLinear)
      << "UpdateTarget is not supported on linear scroll animations.";
  EaseInOutUpdateTarget(t, new_target);
}

void ScrollOffsetAnimationCurve::EaseInOutUpdateTarget(
    base::TimeDelta t,
    const gfx::ScrollOffset& new_target) {
  DCHECK_EQ(animation_type_, AnimationType::kEaseInOut);
  if (std::abs(MaximumDimension(target_value_.DeltaFrom(new_target))) <
      kEpsilon) {
    target_value_ = new_target;
    return;
  }

  base::TimeDelta delayed_by = std::max(base::TimeDelta(), last_retarget_ - t);
  t = std::max(t, last_retarget_);

  gfx::ScrollOffset current_position = GetValue(t);
  gfx::Vector2dF old_delta = target_value_.DeltaFrom(initial_value_);
  gfx::Vector2dF new_delta = new_target.DeltaFrom(current_position);

  // The last segment was of zero duration.
  if ((total_animation_duration_ - last_retarget_).is_zero()) {
    DCHECK_EQ(t, last_retarget_);
    total_animation_duration_ = EaseInOutSegmentDuration(
        new_delta, duration_behavior_.value(), delayed_by);
    target_value_ = new_target;
    return;
  }

  base::TimeDelta old_duration = total_animation_duration_ - last_retarget_;
  double old_normalized_velocity = timing_function_->Velocity(
      ((t - last_retarget_).InSecondsF()) / old_duration.InSecondsF());

  // Use the velocity-based duration bound when it is less than the constant
  // segment duration. This minimizes the "rubber-band" bouncing effect when
  // old_normalized_velocity is large and new_delta is small.
  base::TimeDelta new_duration =
      std::min(EaseInOutSegmentDuration(new_delta, duration_behavior_.value(),
                                        delayed_by),
               VelocityBasedDurationBound(old_delta, old_normalized_velocity,
                                          old_duration, new_delta));

  if (new_duration.InSecondsF() < kEpsilon) {
    // We are already at or very close to the new target. Stop animating.
    target_value_ = new_target;
    total_animation_duration_ = t;
    return;
  }

  // TimingFunction::Velocity gives the slope of the curve from 0 to 1.
  // To match the "true" velocity in px/sec we must adjust this slope for
  // differences in duration and scroll delta between old and new curves.
  double new_normalized_velocity =
      old_normalized_velocity *
      (new_duration.InSecondsF() / old_duration.InSecondsF()) *
      (MaximumDimension(old_delta) / MaximumDimension(new_delta));

  initial_value_ = current_position;
  target_value_ = new_target;
  total_animation_duration_ = t + new_duration;
  last_retarget_ = t;
  timing_function_ = EaseInOutWithInitialVelocity(new_normalized_velocity);
}

}  // namespace cc
