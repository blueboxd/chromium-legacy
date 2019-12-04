// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/drag_window_from_shelf_controller_test_api.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"

namespace ash {

DragWindowFromShelfControllerTestApi::DragWindowFromShelfControllerTestApi(
    DragWindowFromShelfController* window_drag_controller)
    : window_drag_controller_(window_drag_controller) {
  DCHECK(window_drag_controller_);
  window_drag_controller_->AddObserver(this);
}

DragWindowFromShelfControllerTestApi::~DragWindowFromShelfControllerTestApi() {
  window_drag_controller_->RemoveObserver(this);
}

void DragWindowFromShelfControllerTestApi::WaitUntilOverviewIsShown() {
  if (!Shell::Get()->overview_controller()->InOverviewSession() ||
      window_drag_controller_->show_overview_windows()) {
    return;
  }
  show_overview_waiter_ = std::make_unique<base::RunLoop>();
  show_overview_waiter_->Run();
}

void DragWindowFromShelfControllerTestApi::OnOverviewVisibilityChanged(
    bool visible) {
  if (visible && show_overview_waiter_)
    show_overview_waiter_->Quit();
}

}  // namespace ash
