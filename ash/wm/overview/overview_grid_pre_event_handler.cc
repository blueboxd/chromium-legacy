// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_pre_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/events/event.h"

namespace ash {

OverviewGridPreEventHandler::OverviewGridPreEventHandler() = default;

OverviewGridPreEventHandler::~OverviewGridPreEventHandler() = default;

void OverviewGridPreEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_RELEASED)
    HandleClickOrTap(event);
}

void OverviewGridPreEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP)
    HandleClickOrTap(event);
}

void OverviewGridPreEventHandler::HandleClickOrTap(ui::Event* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());
  OverviewController* controller = Shell::Get()->overview_controller();
  if (!controller->InOverviewSession())
    return;
  // Events that happen while app list is sliding out during overview should
  // be ignored to prevent overview from disappearing out from under the user.
  if (!IsSlidingOutOverviewFromShelf())
    controller->EndOverview();
  event->StopPropagation();
}

}  // namespace ash
