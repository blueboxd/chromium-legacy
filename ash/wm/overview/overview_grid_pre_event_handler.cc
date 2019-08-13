// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_pre_event_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/events/event.h"

namespace ash {

namespace {

WallpaperView* GetWallpaperViewForRoot(const aura::Window* root_window) {
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  if (!wallpaper_widget_controller)
    return nullptr;
  return wallpaper_widget_controller->wallpaper_view();
}

}  // namespace

OverviewGridPreEventHandler::OverviewGridPreEventHandler(OverviewGrid* grid)
    : grid_(grid) {
  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->AddPreTargetHandler(this);
}

OverviewGridPreEventHandler::~OverviewGridPreEventHandler() {
  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->RemovePreTargetHandler(this);
}

void OverviewGridPreEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_RELEASED)
    HandleClickOrTap(event);
}

void OverviewGridPreEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      HandleClickOrTap(event);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      if (!ShouldUseTabletModeGridLayout())
        return;
      grid_->StartScroll();
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!ShouldUseTabletModeGridLayout())
        return;
      grid_->UpdateScrollOffset(event->details().scroll_x());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_END: {
      if (!ShouldUseTabletModeGridLayout())
        return;
      grid_->EndScroll();
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void OverviewGridPreEventHandler::HandleClickOrTap(ui::Event* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());
  // Events that happen while app list is sliding out during overview should
  // be ignored to prevent overview from disappearing out from under the user.
  if (!IsSlidingOutOverviewFromShelf())
    Shell::Get()->overview_controller()->EndOverview();
  event->StopPropagation();
}

}  // namespace ash
