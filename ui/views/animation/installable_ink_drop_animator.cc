// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_animator.h"

#include "base/bind.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace views {

// static
constexpr base::TimeDelta InstallableInkDropAnimator::kAnimationDuration;

InstallableInkDropAnimator::InstallableInkDropAnimator(
    InstallableInkDropPainter::State* visual_state,
    gfx::AnimationContainer* animation_container,
    base::RepeatingClosure repaint_callback)
    : visual_state_(visual_state),
      repaint_callback_(repaint_callback),
      highlight_animation_(this) {
  highlight_animation_.SetContainer(animation_container);
  highlight_animation_.SetSlideDuration(kAnimationDuration.InMilliseconds());
}

InstallableInkDropAnimator::~InstallableInkDropAnimator() {
  transition_delay_timer_.Stop();
}

void InstallableInkDropAnimator::AnimateToState(InkDropState target_state) {
  transition_delay_timer_.Stop();

  const InkDropState last_state = target_state_;
  switch (target_state) {
    case InkDropState::HIDDEN:
    case InkDropState::DEACTIVATED:
      target_state_ = InkDropState::HIDDEN;
      visual_state_->activated = false;
      break;
    case InkDropState::ACTION_PENDING:
    case InkDropState::ALTERNATE_ACTION_PENDING:
    case InkDropState::ACTIVATED:
      target_state_ = target_state;
      visual_state_->activated = true;
      break;
    case InkDropState::ACTION_TRIGGERED:
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      if (last_state == InkDropState::ACTION_PENDING ||
          last_state == InkDropState::ALTERNATE_ACTION_PENDING) {
        target_state_ = InkDropState::HIDDEN;
        visual_state_->activated = false;
      } else {
        target_state_ = target_state;
        visual_state_->activated = true;
        transition_delay_timer_.Start(
            FROM_HERE, kAnimationDuration,
            base::Bind(&InstallableInkDropAnimator::AnimateToState,
                       base::Unretained(this), InkDropState::HIDDEN));
      }
      break;
  }

  repaint_callback_.Run();
}

void InstallableInkDropAnimator::AnimateHighlight(bool fade_in) {
  if (fade_in) {
    highlight_animation_.Show();
  } else {
    highlight_animation_.Hide();
  }
}

void InstallableInkDropAnimator::AnimationProgressed(
    const gfx::Animation* animation) {
  visual_state_->highlighted_ratio = highlight_animation_.GetCurrentValue();
  repaint_callback_.Run();
}

}  // namespace views
