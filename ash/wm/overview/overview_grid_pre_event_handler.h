// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_PRE_EVENT_HANDLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_PRE_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ui {
class Event;
class GestureEvent;
class MouseEvent;
}  // namespace ui

namespace ash {

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
class OverviewGridPreEventHandler : public ui::EventHandler {
 public:
  OverviewGridPreEventHandler();
  ~OverviewGridPreEventHandler() override;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void HandleClickOrTap(ui::Event* event);

  DISALLOW_COPY_AND_ASSIGN(OverviewGridPreEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_PRE_EVENT_HANDLER_H_
