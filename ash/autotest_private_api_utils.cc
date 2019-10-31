// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_private_api_utils.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"

namespace ash {

std::vector<aura::Window*> GetAppWindowList() {
  ScopedSkipUserSessionBlockedCheck skip_session_blocked;
  return Shell::Get()->mru_window_tracker()->BuildWindowForCycleWithPipList(
      ash::kAllDesks);
}

ash::HeaderView* GetHeaderViewForWindow(aura::Window* window) {
  return ash::NonClientFrameViewAsh::Get(window)->GetHeaderView();
}

}  // namespace ash