// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_impl.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

namespace {

constexpr char kDeskActivationSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskActivation";
constexpr char kDeskRemovalSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskRemoval";

}  // namespace

// -----------------------------------------------------------------------------
// DeskActivationAnimation:

DeskActivationAnimation::DeskActivationAnimation(DesksController* controller,
                                                 const Desk* ending_desk,
                                                 bool move_left)
    : DeskAnimationBase(controller, ending_desk) {
  for (auto* root : Shell::GetAllRootWindows()) {
    desk_switch_animators_.emplace_back(
        std::make_unique<RootWindowDeskSwitchAnimator>(root, ending_desk, this,
                                                       move_left,
                                                       /*for_remove=*/false));
  }
}

DeskActivationAnimation::~DeskActivationAnimation() = default;

void DeskActivationAnimation::OnStartingDeskScreenshotTakenInternal(
    const Desk* ending_desk) {
  DCHECK_EQ(ending_desk_, ending_desk);
  // The order here matters. Overview must end before ending tablet split view
  // before switching desks. (If clamshell split view is active on one or more
  // displays, then it simply will end when we end overview.) That's because
  // we don't want |TabletModeWindowManager| maximizing all windows because we
  // cleared the snapped ones in |SplitViewController| first. See
  // |TabletModeWindowManager::OnOverviewModeEndingAnimationComplete|.
  // See also test coverage for this case in
  // `TabletModeDesksTest.SnappedStateRetainedOnSwitchingDesksFromOverview`.
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (in_overview) {
    // Exit overview mode immediately without any animations before taking the
    // ending desk screenshot. This makes sure that the ending desk
    // screenshot will only show the windows in that desk, not overview stuff.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEnterExitType::kImmediateExit);
  }
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kDesksChange);

  controller_->ActivateDeskInternal(ending_desk,
                                    /*update_window_activation=*/true);

  MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
}

metrics_util::ReportCallback DeskActivationAnimation::GetReportCallback()
    const {
  return metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
    UMA_HISTOGRAM_PERCENTAGE(kDeskActivationSmoothnessHistogramName,
                             smoothness);
  }));
}

// -----------------------------------------------------------------------------
// DeskRemovalAnimation:

DeskRemovalAnimation::DeskRemovalAnimation(DesksController* controller,
                                           const Desk* desk_to_remove,
                                           const Desk* desk_to_activate,
                                           bool move_left,
                                           DesksCreationRemovalSource source)
    : DeskAnimationBase(controller, desk_to_activate),
      desk_to_remove_(desk_to_remove),
      request_source_(source) {
  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());
  DCHECK_EQ(controller_->active_desk(), desk_to_remove_);

  for (auto* root : Shell::GetAllRootWindows()) {
    desk_switch_animators_.emplace_back(
        std::make_unique<RootWindowDeskSwitchAnimator>(root, desk_to_activate,
                                                       this, move_left,
                                                       /*for_remove=*/true));
  }
}

DeskRemovalAnimation::~DeskRemovalAnimation() = default;

void DeskRemovalAnimation::OnStartingDeskScreenshotTakenInternal(
    const Desk* ending_desk) {
  DCHECK_EQ(ending_desk_, ending_desk);
  DCHECK_EQ(controller_->active_desk(), desk_to_remove_);
  // We are removing the active desk, which may have tablet split view active.
  // We will restore the split view state of the newly activated desk at the
  // end of the animation. Clamshell split view is impossible because
  // |DeskRemovalAnimation| is not used in overview.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kDesksChange);

  // At the end of phase (1), we activate the target desk (i.e. the desk that
  // will be activated after the active desk `desk_to_remove_` is removed).
  // This means that phase (2) will take a screenshot of that desk before we
  // move the windows of `desk_to_remove_` to that target desk.
  controller_->ActivateDeskInternal(ending_desk,
                                    /*update_window_activation=*/false);
}

void DeskRemovalAnimation::OnDeskSwitchAnimationFinishedInternal() {
  // Do the actual desk removal behind the scenes before the screenshot layers
  // are destroyed.
  controller_->RemoveDeskInternal(desk_to_remove_, request_source_);

  MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
}

metrics_util::ReportCallback DeskRemovalAnimation::GetReportCallback() const {
  return ash::metrics_util::ForSmoothness(
      base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kDeskRemovalSmoothnessHistogramName,
                                 smoothness);
      }));
}

}  // namespace ash
