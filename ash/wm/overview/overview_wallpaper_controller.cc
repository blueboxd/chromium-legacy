// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"

namespace ash {

namespace {

// Do not change the wallpaper when entering or exiting overview mode when this
// is true.
bool g_disable_wallpaper_change_for_tests = false;

constexpr base::TimeDelta kBlurSlideDuration =
    base::TimeDelta::FromMilliseconds(250);

bool IsWallpaperChangeAllowed() {
  return !g_disable_wallpaper_change_for_tests;
}

WallpaperWidgetController* GetWallpaperWidgetController(aura::Window* root) {
  return RootWindowController::ForWindow(root)->wallpaper_widget_controller();
}

}  // namespace

// static
void OverviewWallpaperController::SetDoNotChangeWallpaperForTests() {
  g_disable_wallpaper_change_for_tests = true;
}

void OverviewWallpaperController::Blur(bool animate_only) {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(/*should_blur=*/true, animate_only);
}

void OverviewWallpaperController::Unblur() {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(/*should_blur=*/false, /*animate_only=*/true);
}

void OverviewWallpaperController::OnBlurChange(bool should_blur,
                                               bool animate_only) {
  // Don't apply wallpaper change while the session is blocked.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return;

  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    const bool should_animate = ShouldAnimateWallpaper(root);
    // On adding blur, we want to blur immediately if there are no animations
    // and blur after the rest of the overview animations have completed if
    // there is to be wallpaper animations. |OnBlurChange| will get called twice
    // when blurring, but only change the wallpaper when |should_animate|
    // matches |animate_only|.
    if (should_blur && should_animate != animate_only)
      continue;

    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);
    // Tablet mode wallpaper is already dimmed, so no need to change the
    // opacity.
    WallpaperProperty property =
        !should_blur ? wallpaper_constants::kClear
                     : (Shell::Get()->tablet_mode_controller()->InTabletMode()
                            ? wallpaper_constants::kOverviewInTabletState
                            : wallpaper_constants::kOverviewState);
    // TODO(sammiequon): Move this check to wallpaper code.
    if (property == wallpaper_widget_controller->GetWallpaperProperty())
      continue;
    wallpaper_widget_controller->SetWallpaperProperty(
        property, should_animate ? kBlurSlideDuration : base::TimeDelta());
  }
}

}  // namespace ash
