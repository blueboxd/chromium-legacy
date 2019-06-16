// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "ui/compositor/compositor.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// CompositorAnimationRunner::CompositorChecker
//
CompositorAnimationRunner::CompositorChecker::CompositorChecker(
    CompositorAnimationRunner* runner)
    : runner_(runner) {
  scoped_observer_.Add(runner_->compositor_);
}

CompositorAnimationRunner::CompositorChecker::~CompositorChecker() = default;

void CompositorAnimationRunner::CompositorChecker::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  runner_->OnCompositingShuttingDown(compositor);
  DCHECK(!compositor->HasAnimationObserver(runner_));
  scoped_observer_.Remove(compositor);
}

////////////////////////////////////////////////////////////////////////////////
// CompositorAnimationRunner
//
CompositorAnimationRunner::CompositorAnimationRunner(ui::Compositor* compositor)
    : compositor_(compositor) {
  DCHECK(compositor_);
}

CompositorAnimationRunner::~CompositorAnimationRunner() {
  // Make sure we're not observing |compositor_|.
  Stop();
  DCHECK(!compositor_ || !compositor_->HasAnimationObserver(this));
}

void CompositorAnimationRunner::Stop() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);

  min_interval_ = base::TimeDelta::Max();
}

void CompositorAnimationRunner::OnAnimationStep(base::TimeTicks timestamp) {
  if (timestamp - last_tick_ < min_interval_)
    return;

  last_tick_ = timestamp;
  Step(last_tick_);
}

void CompositorAnimationRunner::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  Stop();
  compositor_ = nullptr;
}

void CompositorAnimationRunner::OnStart(base::TimeDelta min_interval) {
  if (!compositor_)
    return;

  last_tick_ = base::TimeTicks::Now();
  min_interval_ = min_interval;
  DCHECK(!compositor_->HasAnimationObserver(this));
  compositor_->AddAnimationObserver(this);
}

}  // namespace views
