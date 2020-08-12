// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
#define ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_

#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desks_histogram_enums.h"

namespace ash {

class Desk;
class DesksController;

class DeskActivationAnimation : public DeskAnimationBase {
 public:
  DeskActivationAnimation(DesksController* controller,
                          const Desk* ending_desk,
                          bool move_left);
  DeskActivationAnimation(const DeskActivationAnimation&) = delete;
  DeskActivationAnimation& operator=(const DeskActivationAnimation&) = delete;
  ~DeskActivationAnimation() override;

  // DeskAnimationBase:
  void OnStartingDeskScreenshotTakenInternal(const Desk* ending_desk) override;
  void OnDeskSwitchAnimationFinishedInternal() override {}
  metrics_util::ReportCallback GetReportCallback() const override;
};

class DeskRemovalAnimation : public DeskAnimationBase {
 public:
  DeskRemovalAnimation(DesksController* controller,
                       const Desk* desk_to_remove,
                       const Desk* desk_to_activate,
                       bool move_left,
                       DesksCreationRemovalSource source);
  DeskRemovalAnimation(const DeskRemovalAnimation&) = delete;
  DeskRemovalAnimation& operator=(const DeskRemovalAnimation&) = delete;
  ~DeskRemovalAnimation() override;

  // DeskAnimationBase:
  void OnStartingDeskScreenshotTakenInternal(const Desk* ending_desk) override;
  void OnDeskSwitchAnimationFinishedInternal() override;
  metrics_util::ReportCallback GetReportCallback() const override;

 private:
  const Desk* const desk_to_remove_;
  const DesksCreationRemovalSource request_source_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_IMPL_H_
