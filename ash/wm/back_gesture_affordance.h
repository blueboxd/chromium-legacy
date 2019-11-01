// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BACK_GESTURE_AFFORDANCE_H_
#define ASH_WM_BACK_GESTURE_AFFORDANCE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/widget/widget.h"

namespace ash {

// This class is responsible for creating, painting, and positioning the back
// gesture affordance.
class ASH_EXPORT BackGestureAffordance : public gfx::AnimationDelegate {
 public:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  explicit BackGestureAffordance(const gfx::Point& location);
  ~BackGestureAffordance() override;

  // Sets the x-axis and y-axis drag progress. 0 means no progress and 1 means
  // full progress. For x-axis progress, values more than 1 are also allowed for
  // resistance animation. Y-axis progress is used to control the y-axis
  // movement distance of the affordance.
  void SetDragProgress(int x_drag_amount, int y_drag_amount);

  // Aborts the affordance and animates it back.
  void Abort();

  // Completes the affordance and fading it out.
  void Complete();

  // Return true if the affordance is activated, which means the drag can be
  // completed to trigger go back.
  bool IsActivated() const;

  gfx::Rect affordance_widget_bounds_for_testing() {
    return affordance_widget_->GetWindowBoundsInScreen();
  }

 private:
  void CreateAffordanceWidget(const gfx::Point& location);

  void UpdateTransform();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // Helper function that returns the affordance progress according to the
  // current values of different progress values (drag progress and abort
  // progress). 1 means the affordance is at the activated state.
  float GetAffordanceProgress() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Widget of the affordance with AffordanceView as the content.
  std::unique_ptr<views::Widget> affordance_widget_;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float drag_progress_ = 0.f;
  float y_drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(BackGestureAffordance);
};

}  // namespace ash

#endif  // ASH_WM_BACK_GESTURE_AFFORDANCE_H_
