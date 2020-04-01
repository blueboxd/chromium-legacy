// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_delegate_views.h"

#include "ui/compositor/animation_metrics_recorder.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

namespace views {

AnimationDelegateViews::AnimationDelegateViews(View* view) : view_(view) {
  if (view)
    scoped_observer_.Add(view);
}

AnimationDelegateViews::~AnimationDelegateViews() {
  // Reset the delegate so that we don't attempt to notify our observer from
  // the destructor.
  if (container_)
    container_->set_observer(nullptr);
}

void AnimationDelegateViews::AnimationContainerWasSet(
    gfx::AnimationContainer* container) {
  if (container_ == container)
    return;

  if (container_)
    container_->set_observer(nullptr);

  container_ = container;
  container_->set_observer(this);
  UpdateAnimationRunner();
}

void AnimationDelegateViews::OnViewAddedToWidget(View* observed_view) {
  UpdateAnimationRunner();
}

void AnimationDelegateViews::OnViewRemovedFromWidget(View* observed_view) {
  UpdateAnimationRunner();
}

void AnimationDelegateViews::OnViewIsDeleting(View* observed_view) {
  scoped_observer_.Remove(view_);
  view_ = nullptr;
  UpdateAnimationRunner();
}

void AnimationDelegateViews::AnimationContainerShuttingDown(
    gfx::AnimationContainer* container) {
  container_ = nullptr;
  compositor_animation_runner_ = nullptr;
}

base::TimeDelta AnimationDelegateViews::GetAnimationDurationForReporting()
    const {
  return base::TimeDelta();
}

void AnimationDelegateViews::SetAnimationMetricsReporter(
    ui::AnimationMetricsReporter* animation_metrics_reporter) {
  if (animation_metrics_reporter_ == animation_metrics_reporter)
    return;

  animation_metrics_reporter_ = animation_metrics_reporter;

  if (!compositor_animation_runner_)
    return;

  compositor_animation_runner_->SetAnimationMetricsReporter(
      animation_metrics_reporter_, GetAnimationDurationForReporting());
}

void AnimationDelegateViews::UpdateAnimationRunner() {
  if (!container_)
    return;

  if (!view_ || !view_->GetWidget() || !view_->GetWidget()->GetCompositor()) {
    // TODO(https://crbug.com/960621): make sure the container has a correct
    // compositor-assisted runner.
    container_->SetAnimationRunner(nullptr);
    compositor_animation_runner_ = nullptr;
    return;
  }

  if (container_->has_custom_animation_runner())
    return;

  auto compositor_animation_runner =
      std::make_unique<CompositorAnimationRunner>(view_->GetWidget());
  compositor_animation_runner_ = compositor_animation_runner.get();
  compositor_animation_runner_->SetAnimationMetricsReporter(
      animation_metrics_reporter_, GetAnimationDurationForReporting());
  container_->SetAnimationRunner(std::move(compositor_animation_runner));
}

}  // namespace views
