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
  return RootWindowController::ForWindow(root_window)
      ->wallpaper_widget_controller()
      ->wallpaper_view();
}

}  // namespace

OverviewGridPreEventHandler::OverviewGridPreEventHandler(OverviewGrid* grid)
    : grid_(grid) {
  GetWallpaperViewForRoot(grid_->root_window())->AddPreTargetHandler(this);
}

OverviewGridPreEventHandler::~OverviewGridPreEventHandler() {
  GetWallpaperViewForRoot(grid_->root_window())->RemovePreTargetHandler(this);
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
      StartDrag(event->location());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!ShouldUseTabletModeGridLayout())
        return;
      UpdateDrag(event->details().scroll_x());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_END: {
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

void OverviewGridPreEventHandler::StartDrag(const gfx::Point& location) {
  grid_->PrepareScrollLimitMin();
}

void OverviewGridPreEventHandler::UpdateDrag(float scroll) {
  // Pass new scroll values to update the offset which will also update overview
  // mode to position windows according to the scroll values.
  grid_->UpdateScrollOffset(scroll);
}

}  // namespace ash
