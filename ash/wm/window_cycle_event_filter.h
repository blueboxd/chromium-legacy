// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_EVENT_FILTER_H_
#define ASH_WM_WINDOW_CYCLE_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_cycle_controller.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"

namespace ash {

// Created by WindowCycleController when cycling through windows. Eats all key
// events and stops cycling when the necessary key sequence is encountered.
// Also allows users to cycle using right/left keys.
class ASH_EXPORT WindowCycleEventFilter : public ui::EventHandler {
 public:
  WindowCycleEventFilter();
  ~WindowCycleEventFilter() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  class AltReleaseHandler : public ui::EventHandler {
   public:
    AltReleaseHandler();
    ~AltReleaseHandler() override;

    // ui::EventHandler:
    void OnKeyEvent(ui::KeyEvent* event) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(AltReleaseHandler);
  };

  // Depending on the values of |event| either repeatedly cycle through windows,
  // stop repeatedly cycling through windows, or cycle once.
  void HandleTriggerKey(ui::KeyEvent* event);

  // Returns whether |event| is a trigger key (tab, left, right, w (when
  // debugging)).
  bool IsTriggerKey(ui::KeyEvent* event) const;

  // Returns whether |event| is an exit key (return, space).
  bool IsExitKey(ui::KeyEvent* event) const;

  // Returns whether the window cycle should repeatedly cycle in the
  // direction given by |event|.
  bool ShouldRepeatKey(ui::KeyEvent* event) const;

  // Returns the direction the window cycle should cycle depending on the
  // combination of keys being pressed.
  WindowCycleController::Direction GetDirection(ui::KeyEvent* event) const;

  // When the user holds Alt+Tab, this timer is used to send repeated
  // cycle commands to WindowCycleController. Note this is not accomplished
  // by marking the Alt+Tab accelerator as "repeatable" in the accelerator
  // table because we wish to control the repeat interval.
  base::RepeatingTimer repeat_timer_;

  AltReleaseHandler alt_release_handler_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleEventFilter);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_EVENT_FILTER_H_
