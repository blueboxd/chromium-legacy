// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/histogram_macros.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

bool InTabletMode() {
  auto* shell = Shell::Get();
  return shell && shell->tablet_mode_controller() &&
         shell->tablet_mode_controller()->InTabletMode();
}

bool IsInSplitView() {
  auto* shell = Shell::Get();
  return shell && shell->split_view_controller() &&
         shell->split_view_controller()->InSplitViewMode();
}

}  // namespace ash
