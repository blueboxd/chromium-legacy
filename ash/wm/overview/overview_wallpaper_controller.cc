// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

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
  OnBlurChange(/*should_blur=*/false, /*animate_only=*/false);
}

void OverviewWallpaperController::OnBlurChange(bool should_blur,
                                               bool animate_only) {
  if (animate_only)
    DCHECK(should_blur);

  // Don't apply wallpaper change while the session is blocked.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return;

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    // |overview_session| may be null on overview exit because we call this
    // after the animations are done running. We don't support animation on exit
    // so just set |should_animate| to false.
    OverviewGrid* grid = overview_session
                             ? overview_session->GetGridWithRootWindow(root)
                             : nullptr;
    bool should_animate = grid && grid->ShouldAnimateWallpaper();
    if (should_animate != animate_only)
      continue;

    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);
    // Tablet mode wallpaper is already dimmed, so no need to change the
    // opacity.
    WallpaperProperty property =
        !should_blur ? wallpaper_constants::kClear
                     : (Shell::Get()->tablet_mode_controller()->InTabletMode()
                            ? wallpaper_constants::kOverviewInTabletState
                            : wallpaper_constants::kOverviewState);
    wallpaper_widget_controller->SetWallpaperProperty(
        property, should_animate ? kBlurSlideDuration : base::TimeDelta());
  }
}

}  // namespace ash
