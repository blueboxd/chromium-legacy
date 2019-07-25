// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_focus_cycler.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

ShelfFocusCycler::ShelfFocusCycler(Shelf* shelf) : shelf_(shelf) {}

void ShelfFocusCycler::FocusOut(bool reverse, SourceView source_view) {
  switch (source_view) {
    case SourceView::kShelfView: {
      StatusAreaWidget* status_area_widget = shelf_->GetStatusAreaWidget();
      status_area_widget->status_area_widget_delegate()
          ->set_default_last_focusable_child(reverse);
      Shell::Get()->focus_cycler()->FocusWidget(status_area_widget);
      break;
    }
    case SourceView::kSystemTrayView: {
      // Focus can move to the shelf, or the focus will be delegated to system
      // tray focus observers.
      bool should_focus_shelf = true;
      // If we are using a views-based shelf:
      // * If we're in an active session, always bring focus to the shelf
      //   whether we are going in reverse or not.
      // * Otherwise (login/lock screen, OOBE), bring focus to the shelf only
      //   if we're going in reverse; if we're going forward, let the system
      //   tray focus observers focus the lock/login view.
      ShelfWidget* shelf_widget = shelf_->shelf_widget();
      if (shelf_widget->login_shelf_view()->GetVisible()) {
        // Login/lock screen or OOBE.
        should_focus_shelf = reverse;
      }

      if (should_focus_shelf) {
        shelf_widget->set_default_last_focusable_child(reverse);
        Shell::Get()->focus_cycler()->FocusWidget(shelf_widget);
        shelf_widget->FocusFirstOrLastFocusableChild(reverse);
      } else {
        Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
      }
      break;
    }
  }
}
}  // namespace ash
