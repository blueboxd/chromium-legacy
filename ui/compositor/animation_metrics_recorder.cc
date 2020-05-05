// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_metrics_recorder.h"

#include "base/check_op.h"
#include "ui/compositor/animation_metrics_reporter.h"

namespace ui {

AnimationMetricsRecorder::AnimationMetricsRecorder(
    AnimationMetricsReporter* reporter)
    : reporter_(reporter) {}

AnimationMetricsRecorder::~AnimationMetricsRecorder() = default;

void AnimationMetricsRecorder::OnAnimatorAttached(
    base::Optional<int> frame_number) {
  DCHECK(frame_number.has_value());
  if (start_frame_number_.has_value() || animator_detached_after_start_)
    return;

  start_frame_number_ = frame_number;
}

void AnimationMetricsRecorder::OnAnimatorDetached() {
  animator_detached_after_start_ = true;
  start_frame_number_.reset();
}

void AnimationMetricsRecorder::OnAnimationStart(
    base::Optional<int> start_frame_number,
    base::TimeTicks effective_start_time,
    base::TimeDelta duration) {
  start_frame_number_ = start_frame_number;
  effective_start_time_ = effective_start_time;
  duration_ = duration;
  animator_detached_after_start_ = false;
}

void AnimationMetricsRecorder::OnAnimationEnd(
    base::Optional<int> end_frame_number,
    float refresh_rate) {
  DCHECK(reporter_);

  if (duration_.is_zero() || !start_frame_number_.has_value() ||
      !end_frame_number.has_value()) {
    return;
  }
  DCHECK_GE(end_frame_number.value(), start_frame_number_.value());

  base::TimeDelta elapsed = base::TimeTicks::Now() - effective_start_time_;
  if (elapsed < duration_)
    return;

  int smoothness = 100;
  const float frame_interval =
      base::Time::kMillisecondsPerSecond / refresh_rate;
  const float actual_duration =
      (end_frame_number.value() - start_frame_number_.value()) * frame_interval;
  if (duration_.InMillisecondsF() - actual_duration >= frame_interval)
    smoothness = 100 * (actual_duration / duration_.InMillisecondsF());
  reporter_->Report(smoothness);
}

}  // namespace ui
