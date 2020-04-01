// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "ui/compositor/animation_metrics_recorder.h"
#include "ui/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// CompositorAnimationRunner
//

CompositorAnimationRunner::CompositorAnimationRunner(Widget* widget)
    : widget_(widget) {
  widget_->AddObserver(this);
}

CompositorAnimationRunner::~CompositorAnimationRunner() {
  // Make sure we're not observing |compositor_|.
  if (widget_)
    OnWidgetDestroying(widget_);
  DCHECK(!compositor_ || !compositor_->HasAnimationObserver(this));
}

void CompositorAnimationRunner::SetAnimationMetricsReporter(
    ui::AnimationMetricsReporter* animation_metrics_reporter,
    base::TimeDelta expected_duration) {
  if (animation_metrics_reporter) {
    DCHECK(!expected_duration.is_zero());
    animation_metrics_recorder_ =
        std::make_unique<ui::AnimationMetricsRecorder>(
            animation_metrics_reporter);
    expected_duration_ = expected_duration;
  } else {
    animation_metrics_recorder_.reset();
  }
}

void CompositorAnimationRunner::Stop() {
  // Record metrics if necessary.
  if (animation_metrics_recorder_ && compositor_) {
    animation_metrics_recorder_->OnAnimationEnd(
        compositor_->activated_frame_count(), compositor_->refresh_rate());
  }

  StopInternal();
}

void CompositorAnimationRunner::OnAnimationStep(base::TimeTicks timestamp) {
  if (timestamp - last_tick_ < min_interval_)
    return;

  last_tick_ = timestamp;
  Step(last_tick_);
}

void CompositorAnimationRunner::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  StopInternal();
}

void CompositorAnimationRunner::OnWidgetDestroying(Widget* widget) {
  StopInternal();
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void CompositorAnimationRunner::OnStart(base::TimeDelta min_interval,
                                        base::TimeDelta elapsed) {
  if (!widget_)
    return;

  ui::Compositor* current_compositor = widget_->GetCompositor();
  if (!current_compositor) {
    StopInternal();
    return;
  }

  if (current_compositor != compositor_) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = current_compositor;
  }

  last_tick_ = base::TimeTicks::Now() - elapsed;
  min_interval_ = min_interval;
  DCHECK(!compositor_->HasAnimationObserver(this));
  compositor_->AddAnimationObserver(this);

  if (animation_metrics_recorder_) {
    animation_metrics_recorder_->OnAnimationStart(
        compositor_->activated_frame_count(), last_tick_, expected_duration_);
  }
}

void CompositorAnimationRunner::StopInternal() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);

  min_interval_ = base::TimeDelta::Max();
  compositor_ = nullptr;
}

}  // namespace views
