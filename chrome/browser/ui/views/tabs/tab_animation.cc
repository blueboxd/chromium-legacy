// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_animation.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "ui/gfx/animation/tween.h"

namespace {

constexpr base::TimeDelta kZeroDuration = base::TimeDelta::FromMilliseconds(0);

}  // namespace

constexpr base::TimeDelta TabAnimation::kAnimationDuration;

TabAnimation::TabAnimation(ViewType view_type,
                           TabAnimationState initial_state,
                           TabAnimationState target_state,
                           base::TimeDelta duration,
                           base::OnceClosure tab_removed_callback)
    : view_type_(view_type),
      initial_state_(initial_state),
      target_state_(target_state),
      start_time_(base::TimeTicks::Now()),
      duration_(duration),
      tab_removed_callback_(std::move(tab_removed_callback)) {}

TabAnimation::~TabAnimation() = default;

TabAnimation::TabAnimation(TabAnimation&&) noexcept = default;
TabAnimation& TabAnimation::operator=(TabAnimation&&) noexcept = default;

// static
TabAnimation TabAnimation::ForStaticState(
    ViewType view_type,
    TabAnimationState static_state,
    base::OnceClosure tab_removed_callback) {
  return TabAnimation(view_type, static_state, static_state, kZeroDuration,
                      std::move(tab_removed_callback));
}

void TabAnimation::AnimateTo(TabAnimationState target_state) {
  initial_state_ = GetCurrentState();
  target_state_ = target_state;
  start_time_ = base::TimeTicks::Now();
  duration_ = kAnimationDuration;
}

void TabAnimation::RetargetTo(TabAnimationState target_state) {
  base::TimeDelta duration = GetTimeRemaining();

  initial_state_ = GetCurrentState();
  target_state_ = target_state;
  start_time_ = base::TimeTicks::Now();
  duration_ = duration;
}

void TabAnimation::CompleteAnimation() {
  initial_state_ = target_state_;
  start_time_ = base::TimeTicks::Now();
  duration_ = kZeroDuration;
}

void TabAnimation::NotifyCloseCompleted() {
  std::move(tab_removed_callback_).Run();
}

TabAnimationState TabAnimation::GetCurrentState() const {
  if (duration_.is_zero())
    return target_state_;

  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  const double normalized_elapsed_time = base::ClampToRange(
      elapsed_time.InMillisecondsF() / duration_.InMillisecondsF(), 0.0, 1.0);
  const double interpolation_value = gfx::Tween::CalculateValue(
      gfx::Tween::Type::EASE_OUT, normalized_elapsed_time);
  return TabAnimationState::Interpolate(interpolation_value, initial_state_,
                                        target_state_);
}

base::TimeDelta TabAnimation::GetTimeRemaining() const {
  return std::max(start_time_ + duration_ - base::TimeTicks::Now(),
                  kZeroDuration);
}
