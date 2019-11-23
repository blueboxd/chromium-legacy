// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_event_filter.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/shell.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_cycle_list.h"
#include "base/bind.h"
#include "ui/events/event.h"

namespace ash {

WindowCycleEventFilter::WindowCycleEventFilter() {
  Shell::Get()->AddPreTargetHandler(this);
  // Handling release of "Alt" must come before other pretarget handlers
  // (specifically, the partial screenshot handler). See crbug.com/651939
  // We can't do all key event handling that early though because it prevents
  // other accelerators (like triggering a partial screenshot) from working.
  Shell::Get()->AddPreTargetHandler(&alt_release_handler_,
                                    ui::EventTarget::Priority::kSystem);
}

WindowCycleEventFilter::~WindowCycleEventFilter() {
  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->RemovePreTargetHandler(&alt_release_handler_);
}

void WindowCycleEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Until the alt key is released, all key events except the trigger key press
  // (which is handled by the accelerator controller to call Step) are handled
  // by this window cycle controller: https://crbug.com/340339.
  bool is_trigger_key = event->key_code() == ui::VKEY_TAB ||
                        (debug::DeveloperAcceleratorsEnabled() &&
                         event->key_code() == ui::VKEY_W);
  if (!is_trigger_key || event->type() != ui::ET_KEY_PRESSED)
    event->StopPropagation();
  if (is_trigger_key) {
    if (event->type() == ui::ET_KEY_RELEASED) {
      repeat_timer_.Stop();
    } else if (event->type() == ui::ET_KEY_PRESSED && event->is_repeat() &&
               !repeat_timer_.IsRunning()) {
      repeat_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(180),
          base::BindRepeating(
              &WindowCycleController::HandleCycleWindow,
              base::Unretained(Shell::Get()->window_cycle_controller()),
              event->IsShiftDown() ? WindowCycleController::BACKWARD
                                   : WindowCycleController::FORWARD));
    }
  } else if (event->key_code() == ui::VKEY_ESCAPE) {
    Shell::Get()->window_cycle_controller()->CancelCycling();
  }
}

void WindowCycleEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  // Prevent mouse clicks from doing anything while the Alt+Tab UI is active
  // <crbug.com/641171> but don't interfere with drag and drop operations
  // <crbug.com/660945>.
  if (event->type() != ui::ET_MOUSE_DRAGGED &&
      event->type() != ui::ET_MOUSE_RELEASED) {
    event->StopPropagation();
  }
}

WindowCycleEventFilter::AltReleaseHandler::AltReleaseHandler() = default;

WindowCycleEventFilter::AltReleaseHandler::~AltReleaseHandler() = default;

void WindowCycleEventFilter::AltReleaseHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace ash
