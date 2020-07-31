// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_ANIMATION_METRICS_REPORTER_H_
#define UI_COMPOSITOR_ANIMATION_METRICS_REPORTER_H_

#include "ui/compositor/compositor_export.h"

namespace ui {

// Override this class and attach it to any class that supports recording
// animation smoothness (e.g. ui::LayerAnimationSequence,
// views::CompositorAnimationRunner). When an animation ends, |Report| will be
// called with the animation smoothness as a percentage.
class COMPOSITOR_EXPORT AnimationMetricsReporter {
 public:
  virtual ~AnimationMetricsReporter() = default;
  // Called at the end of every animation sequence, if the duration and frames
  // passed meets certain criteria. |value| is the smoothness, measured in
  // percentage of the animation.
  virtual void Report(int value) = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_ANIMATION_METRICS_REPORTER_H_
