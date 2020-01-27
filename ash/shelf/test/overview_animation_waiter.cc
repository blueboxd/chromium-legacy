// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/overview_animation_waiter.h"
#include "ash/wm/overview/overview_controller.h"

namespace ash {

OverviewAnimationWaiter::OverviewAnimationWaiter() {
  Shell::Get()->overview_controller()->AddObserver(this);
}

OverviewAnimationWaiter::~OverviewAnimationWaiter() {
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void OverviewAnimationWaiter::Wait() {
  run_loop_.Run();
}

void OverviewAnimationWaiter::OnOverviewModeStartingAnimationComplete(
    bool cancel) {
  run_loop_.Quit();
}

void OverviewAnimationWaiter::OnOverviewModeEndingAnimationComplete(
    bool cancel) {
  run_loop_.Quit();
}

}  // namespace ash
