// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_
#define ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

class ContextualNudge;
class DragHandle;
class Shelf;

// Handles the swipe up gesture on login shelf. The gesture is enabled only when
// the login screen stack registers a handler for the swipe gesture.
// Currently, the handler may be set during user first run flow on the final
// screen of the flow (where swipe up will finalize user setup flow and start
// the user session).
class ASH_EXPORT LoginShelfGestureController : public views::WidgetObserver {
 public:
  LoginShelfGestureController(Shelf* shelf,
                              DragHandle* drag_handle,
                              const base::string16& gesture_nudge,
                              const base::RepeatingClosure fling_handler,
                              base::OnceClosure exit_handler);
  LoginShelfGestureController(const LoginShelfGestureController& other) =
      delete;
  LoginShelfGestureController& operator=(
      const LoginShelfGestureController& other) = delete;
  ~LoginShelfGestureController() override;

  // Handles a gesture event on the login shelf.
  // Returns whether the controller handled the event.
  // The controller will handle SCROLL_BEGIN and SCROLL_UPDATE events if the
  // scroll direction changes towards the top of the screen (and is within the
  // shelf bounds).
  // SCROLL_END and SCROLL_FLING_START will be only handled if a SCROLL_BEGIN or
  // SCROLL_UPDATE was handled (i.e. if |active_| is true).
  bool HandleGestureEvent(const ui::GestureEvent& event_in_screen);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  ContextualNudge* nudge_for_testing() { return nudge_; }

 private:
  // Starts handling gesture drag if it's a start of upward swipe from the
  // shelf.
  bool MaybeStartGestureDrag(const ui::GestureEvent& event_in_screen);

  // Ends gesture drag, and runs fling_handler_ if the gesture was detected to
  // be upward fling from the shelf.
  void EndDrag(const ui::GestureEvent& event_in_screen);

  // Whether a gesture drag is being handled by the controller.
  bool active_ = false;

  Shelf* const shelf_;

  // The contextual nudge bubble for letting the user know they can swipe up to
  // perform an action.
  // It's a bubble dialog widget delegate, deleted when its widget is destroyed,
  // and the widget is owned by the window hierarchy.
  ContextualNudge* nudge_ = nullptr;

  // The callback to be run whenever swipe from shelf is detected.
  base::RepeatingClosure const fling_handler_;

  // Called when the swipe controller gets reset (at which point swipe from the
  // login shelf gesture will be disabled).
  base::OnceClosure exit_handler_;
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_GESTURE_CONTROLLER_H_
