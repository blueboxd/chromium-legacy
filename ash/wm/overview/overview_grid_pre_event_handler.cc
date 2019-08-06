// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_pre_event_handler.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_util.h"
#include "ui/events/event.h"

namespace ash {

OverviewGridPreEventHandler::OverviewGridPreEventHandler() = default;

OverviewGridPreEventHandler::~OverviewGridPreEventHandler() = default;

void OverviewGridPreEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_RELEASED)
    HandleClickOrTap(event);
}

void OverviewGridPreEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(sammiequon): Investigate why scrolling on the bottom of the grid exits
  // overview.
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
  auto* controller = Shell::Get()->overview_controller();
  if (!controller->InOverviewSession())
    return;
  // Events that happen while app list is sliding out during overview should
  // be ignored to prevent overview from disappearing out from under the user.
  if (!IsSlidingOutOverviewFromShelf())
    controller->EndOverview();
  event->StopPropagation();
}

void OverviewGridPreEventHandler::StartDrag(const gfx::Point& location) {
  // TODO(sammiequon): Investigate if this can be passed in the constructor.
  auto* controller = Shell::Get()->overview_controller();
  if (!controller->InOverviewSession())
    return;
  grid_ = controller->overview_session()->GetGridWithRootWindow(
      window_util::GetRootWindowAt(location));
  grid_->PrepareScrollLimitMin();
}

void OverviewGridPreEventHandler::UpdateDrag(float scroll) {
  // Pass new scroll values to update the offset which will also update overview
  // mode to position windows according to the scroll values.
  grid_->UpdateScrollOffset(scroll);
}

}  // namespace ash
