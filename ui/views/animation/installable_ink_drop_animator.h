// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {

class InstallableInkDropPainter;

// Manages animating the ink drop's visual state. This class is essentially a
// state machine, using the current and target InkDropStates to affect the
// InstallableInkDropPainter passed in. The animations are currently minimal.
class VIEWS_EXPORT InstallableInkDropAnimator {
 public:
  static constexpr base::TimeDelta kAnimationDuration =
      base::TimeDelta::FromMilliseconds(500);

  explicit InstallableInkDropAnimator(InstallableInkDropPainter* painter,
                                      base::RepeatingClosure repaint_callback);
  ~InstallableInkDropAnimator();

  // Set the target state and animate to it.
  void AnimateToState(InkDropState target_state);

  InkDropState target_state() const { return target_state_; }

 private:
  // The painter whose state we are controlling.
  InstallableInkDropPainter* const painter_;

  // Called when |painter_| is modified so that the owner can repaint it.
  base::RepeatingClosure repaint_callback_;

  InkDropState target_state_ = InkDropState::HIDDEN;

  base::OneShotTimer transition_delay_timer_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
