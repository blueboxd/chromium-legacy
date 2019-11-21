// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve_factory.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/timing_function.h"

namespace cc {
namespace {
ScrollOffsetAnimationCurve::DurationBehavior GetDurationBehaviorFromScrollType(
    ScrollOffsetAnimationCurveFactory::ScrollType scroll_type) {
  switch (scroll_type) {
    case ScrollOffsetAnimationCurveFactory::ScrollType::kProgrammatic:
      return ScrollOffsetAnimationCurve::DurationBehavior::DELTA_BASED;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kKeyboard:
      return ScrollOffsetAnimationCurve::DurationBehavior::CONSTANT;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kMouseWheel:
      return ScrollOffsetAnimationCurve::DurationBehavior::INVERSE_DELTA;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kAutoScroll:
      NOTREACHED();
      return ScrollOffsetAnimationCurve::DurationBehavior::DELTA_BASED;
  }
}
}  // namespace

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateAnimation(
    const gfx::ScrollOffset& target_value,
    ScrollType scroll_type) {
  return (scroll_type == ScrollType::kAutoScroll)
             ? CreateLinearAnimation(target_value)
             : CreateEaseInOutAnimation(
                   target_value,
                   GetDurationBehaviorFromScrollType(scroll_type));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
    const gfx::ScrollOffset& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return CreateEaseInOutAnimation(target_value, duration_behavior);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimationForTesting(
    const gfx::ScrollOffset& target_value) {
  return CreateLinearAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimation(
    const gfx::ScrollOffset& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value,
      CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT),
      ScrollOffsetAnimationCurve::AnimationType::kEaseInOut,
      duration_behavior));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimation(
    const gfx::ScrollOffset& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, LinearTimingFunction::Create(),
      ScrollOffsetAnimationCurve::AnimationType::kLinear));
}
}  // namespace cc
