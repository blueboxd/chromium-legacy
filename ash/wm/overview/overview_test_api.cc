// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/overview_test_api.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_animation_state_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/callback.h"

namespace ash {

OverviewTestApi::OverviewTestApi() = default;
OverviewTestApi::~OverviewTestApi() = default;

void OverviewTestApi::SetOverviewMode(
    bool start,
    OverviewTestApi::DoneCallback done_callback) {
  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview_session = overview_controller->InOverviewSession();

  if (start && in_overview_session &&
      !overview_controller->IsInStartAnimation()) {
    std::move(done_callback).Run(true);
    return;
  }
  if (!start && !in_overview_session &&
      !overview_controller->IsCompletingShutdownAnimations()) {
    std::move(done_callback).Run(true);
    return;
  }

  auto* waiter = new OverviewAnimationStateWaiter(
      start ? OverviewAnimationState::kEnterAnimationComplete
            : OverviewAnimationState::kExitAnimationComplete,
      std::move(done_callback));

  const bool animation_started = start ? overview_controller->StartOverview()
                                       : overview_controller->EndOverview();

  if (!animation_started)
    waiter->Cancel();
}

}  // namespace ash
