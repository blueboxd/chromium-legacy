// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_BASE_H_
#define ASH_WM_DESKS_DESK_ANIMATION_BASE_H_

#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ui/compositor/throughput_tracker.h"

namespace ash {

class Desk;
class DesksController;

// An abstract class that handles the shared operations need to be performed
// when doing an animation that causes a desk switch animation. Subclasses
// such as DeskActivationAnimation and DeskRemovalAnimation implement the
// abstract interface of this class to handle the unique operations specific to
// each animation type.
class DeskAnimationBase : public RootWindowDeskSwitchAnimator::Delegate {
 public:
  DeskAnimationBase(DesksController* controller, const Desk* ending_desk);
  DeskAnimationBase(const DeskAnimationBase&) = delete;
  DeskAnimationBase& operator=(const DeskAnimationBase&) = delete;
  ~DeskAnimationBase() override;

  const Desk* ending_desk() const { return ending_desk_; }

  // Launches the animation. This should be done once all animators
  // are created and added to `desk_switch_animators_`. This is to avoid any
  // potential race conditions that might happen if one animator finished phase
  // (1) of the animation while other animators are still being constructed.
  void Launch();

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(const Desk* ending_desk) override;
  void OnEndingDeskScreenshotTaken() override;
  void OnDeskSwitchAnimationFinished() override;

 protected:
  // Abstract functions that can be overridden by child classes to do different
  // things when phase (1), and phase (3) completes. Note that
  // `OnDeskSwitchAnimationFinishedInternal()` will be called before the desks
  // screenshot layers, stored in `desk_switch_animators_`, are destroyed.
  virtual void OnStartingDeskScreenshotTakenInternal(
      const Desk* ending_desk) = 0;
  virtual void OnDeskSwitchAnimationFinishedInternal() = 0;

  // Since performance here matters, we have to use the UMA histograms macros to
  // report the smoothness histograms, but each macro use has to be associated
  // with exactly one histogram name. This function allows subclasses to return
  // a callback that reports the histogram using the macro with their desired
  // name.
  virtual metrics_util::ReportCallback GetReportCallback() const = 0;

  DesksController* const controller_;

  // An animator object per each root. Once all the animations are complete,
  // this list is cleared.
  std::vector<std::unique_ptr<RootWindowDeskSwitchAnimator>>
      desk_switch_animators_;

  // The desk that will be active after this animation ends.
  const Desk* const ending_desk_;

 private:
  // ThroughputTracker used for measuring this animation smoothness.
  ui::ThroughputTracker throughput_tracker_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_BASE_H_
