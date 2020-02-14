// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_ANIMATION_METRICS_RECORDER_H_
#define UI_COMPOSITOR_ANIMATION_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace ui {

class AnimationMetricsReporter;

class AnimationMetricsRecorder {
 public:
  explicit AnimationMetricsRecorder(AnimationMetricsReporter* reporter);
  ~AnimationMetricsRecorder();

  AnimationMetricsRecorder(const AnimationMetricsRecorder&) = delete;
  AnimationMetricsRecorder& operator=(const AnimationMetricsRecorder&) = delete;

  void OnAnimationStart(int start_frame_number,
                        base::TimeTicks effective_start_time,
                        base::TimeDelta duration);
  void OnAnimationEnd(int end_frame_number, float refresh_rate);

 private:
  AnimationMetricsReporter* const reporter_;

  // Variables set at the start of an animation which are required to compute
  // the smoothness when the animation ends.
  int start_frame_number_ = 0;
  base::TimeTicks effective_start_time_;
  base::TimeDelta duration_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_ANIMATION_METRICS_RECORDER_H_
