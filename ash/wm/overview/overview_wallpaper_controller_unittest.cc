// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_property.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"

namespace ash {

namespace {

WallpaperProperty GetWallpaperProperty(const aura::Window* window) {
  return RootWindowController::ForWindow(window)
      ->wallpaper_widget_controller()
      ->GetWallpaperProperty();
}

void CheckWallpaperProperty(WallpaperProperty expected) {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows())
    EXPECT_EQ(expected, GetWallpaperProperty(root));
}

}  // namespace

using OverviewWallpaperControllerTest = AshTestBase;

// Tests that entering overview in clamshell mode and toggling to tablet mode
// updates the wallpaper window property correctly.
TEST_F(OverviewWallpaperControllerTest, OverviewToggleEnterTabletMode) {
  CheckWallpaperProperty(wallpaper_constants::kClear);

  ToggleOverview();
  CheckWallpaperProperty(wallpaper_constants::kOverviewState);

  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperProperty(wallpaper_constants::kOverviewInTabletState);

  ToggleOverview();
  CheckWallpaperProperty(wallpaper_constants::kClear);
}

// Tests that entering overview in tablet mode and toggling to clamshell mode
// updates the wallpaper window property correctly.
TEST_F(OverviewWallpaperControllerTest, OverviewToggleLeaveTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();
  CheckWallpaperProperty(wallpaper_constants::kClear);

  ToggleOverview();
  CheckWallpaperProperty(wallpaper_constants::kOverviewInTabletState);

  TabletModeControllerTestApi().LeaveTabletMode();
  CheckWallpaperProperty(wallpaper_constants::kOverviewState);

  ToggleOverview();
  CheckWallpaperProperty(wallpaper_constants::kClear);
}

}  // namespace ash
