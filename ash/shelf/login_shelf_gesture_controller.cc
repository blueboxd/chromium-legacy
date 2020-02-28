// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_gesture_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_nudge.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The upward velocity threshold for the swipe up from the login shelf to be
// reported as fling gesture.
constexpr float kVelocityToHomeScreenThreshold = 1000.f;

}  // namespace

LoginShelfGestureController::LoginShelfGestureController(
    Shelf* shelf,
    DragHandle* drag_handle,
    const base::string16& gesture_nudge,
    const base::RepeatingClosure fling_handler,
    base::OnceClosure exit_handler)
    : shelf_(shelf),
      fling_handler_(fling_handler),
      exit_handler_(std::move(exit_handler)) {
  DCHECK(fling_handler_);
  DCHECK(exit_handler_);

  const bool is_oobe = Shell::Get()->session_controller()->GetSessionState() ==
                       session_manager::SessionState::OOBE;
  const SkColor nudge_text_color =
      is_oobe ? gfx::kGoogleGrey700 : gfx::kGoogleGrey100;
  nudge_ = new ContextualNudge(drag_handle, nullptr /*parent_window*/,
                               ContextualNudge::Position::kTop, gfx::Insets(4),
                               gesture_nudge, nudge_text_color);
  nudge_->GetWidget()->Show();
  nudge_->GetWidget()->AddObserver(this);
}

LoginShelfGestureController::~LoginShelfGestureController() {
  if (nudge_) {
    nudge_->GetWidget()->RemoveObserver(this);
    nudge_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
  nudge_ = nullptr;

  std::move(exit_handler_).Run();
}

bool LoginShelfGestureController::HandleGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return MaybeStartGestureDrag(event_in_screen);

  // If the previous events in the gesture sequence did not start handling the
  // gesture, try again.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE)
    return active_ || MaybeStartGestureDrag(event_in_screen);

  if (!active_)
    return false;

  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    EndDrag(event_in_screen);
    return true;
  }

  // Ending non-fling gesture, or unexpected event (if different than
  // SCROLL_END), mark the controller as inactive, but report the event as
  // handled in the former case only.
  active_ = false;
  return event_in_screen.type() == ui::ET_GESTURE_SCROLL_END;
}

void LoginShelfGestureController::OnWidgetDestroying(views::Widget* widget) {
  nudge_ = nullptr;
}

bool LoginShelfGestureController::MaybeStartGestureDrag(
    const ui::GestureEvent& event_in_screen) {
  DCHECK(event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
         event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE);

  // Ignore downward swipe for scroll begin.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      event_in_screen.details().scroll_y_hint() >= 0) {
    return false;
  }

  // Ignore downward swipe for scroll update.
  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE &&
      event_in_screen.details().scroll_y() >= 0) {
    return false;
  }

  // Ignore swipes that are outside of the shelf bounds.
  if (event_in_screen.location().y() <
      shelf_->shelf_widget()->GetWindowBoundsInScreen().y()) {
    return false;
  }

  active_ = true;
  return true;
}

void LoginShelfGestureController::EndDrag(
    const ui::GestureEvent& event_in_screen) {
  DCHECK_EQ(event_in_screen.type(), ui::ET_SCROLL_FLING_START);

  active_ = false;

  // If the drag ends below the shelf, do not go to home screen (theoratically
  // it may happen in kExtended hotseat case when drag can start and end below
  // the shelf).
  if (event_in_screen.location().y() >=
      shelf_->shelf_widget()->GetWindowBoundsInScreen().y()) {
    return;
  }

  const int velocity_y = event_in_screen.details().velocity_y();
  if (velocity_y > -kVelocityToHomeScreenThreshold)
    return;

  fling_handler_.Run();
}

}  // namespace ash
