// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace gfx {
class AnimationContainer;
}

namespace views {

// Manages animating the ink drop's visual state. This class is essentially a
// state machine, using the current and target InkDropStates to affect the
// InstallableInkDropPainter passed in. The animations are currently minimal.
class VIEWS_EXPORT InstallableInkDropAnimator : public gfx::AnimationDelegate {
 public:
  static constexpr base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromMilliseconds(500);

  // We use a shared gfx::AnimationContainer for our animations to allow them to
  // update in sync. It is passed in at construction for two reasons: it allows
  // |views::CompositorAnimationRunner| to be used for more efficient and less
  // janky animations, and it enables easier unit testing.
  explicit InstallableInkDropAnimator(
      InstallableInkDropPainter::State* visual_state,
      gfx::AnimationContainer* animation_container,
      base::RepeatingClosure repaint_callback);
  ~InstallableInkDropAnimator() override;

  // Set the target state and animate to it.
  void AnimateToState(InkDropState target_state);

  // Animates the hover highlight in or out. Animates in if |fade_in| is true,
  // and out otherwise.
  void AnimateHighlight(bool fade_in);

  InkDropState target_state() const { return target_state_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // The visual state we are controlling.
  InstallableInkDropPainter::State* const visual_state_;

  // Called when |visual_state_| changes so the user can repaint.
  base::RepeatingClosure repaint_callback_;

  InkDropState target_state_ = InkDropState::HIDDEN;

  // Used to animate the painter's highlight value in and out.
  gfx::SlideAnimation highlight_animation_;

  base::OneShotTimer transition_delay_timer_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
