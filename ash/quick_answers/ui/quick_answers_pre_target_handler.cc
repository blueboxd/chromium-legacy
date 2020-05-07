// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_pre_target_handler.h"

#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/shell.h"
#include "base/containers/adapters.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

QuickAnswersPreTargetHandler::QuickAnswersPreTargetHandler(
    QuickAnswersView* quick_answers_view)
    : quick_answers_view_(quick_answers_view) {
  // QuickAnswersView is a companion view of a menu. Menu host widget sets
  // mouse capture as well as a pre-target handler, so we need to register one
  // here as well to intercept events for QuickAnswersView.
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

QuickAnswersPreTargetHandler::~QuickAnswersPreTargetHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void QuickAnswersPreTargetHandler::OnEvent(ui::Event* event) {
  if (!event->IsLocatedEvent())
    return;

  // Clone event to forward down the view-hierarchy.
  auto clone = ui::Event::Clone(*event);
  ui::Event::DispatcherApi(clone.get()).set_target(event->target());
  auto* to_dispatch = clone->AsLocatedEvent();
  auto location = to_dispatch->target()->GetScreenLocation(*to_dispatch);

  // `ET_MOUSE_MOVED` events outside the top-view's bounds are also dispatched
  // to clear any set hover-state.
  bool dispatch_event =
      (quick_answers_view_->GetBoundsInScreen().Contains(location) ||
       to_dispatch->type() == ui::EventType::ET_MOUSE_MOVED);
  if (dispatch_event) {
    // Convert to local coordinates and forward to the top-view.
    views::View::ConvertPointFromScreen(quick_answers_view_, &location);
    to_dispatch->set_location(location);
    ui::Event::DispatcherApi(to_dispatch).set_target(quick_answers_view_);

    // Convert touch-event to gesture before dispatching since views do not
    // process touch-events.
    std::unique_ptr<ui::GestureEvent> gesture_event;
    if (to_dispatch->type() == ui::ET_TOUCH_PRESSED) {
      gesture_event = std::make_unique<ui::GestureEvent>(
          to_dispatch->x(), to_dispatch->y(), ui::EF_NONE,
          base::TimeTicks::Now(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
      to_dispatch = gesture_event.get();
    }

    DoDispatchEvent(quick_answers_view_, to_dispatch);

    // Clicks outside menu-bounds (including those inside QuickAnswersView)
    // can dismiss the menu. Some click-events, like those meant for the
    // retry-button, should not be propagated to the menu to prevent so.
    if (quick_answers_view_->preempt_last_click_event())
      event->StopPropagation();
  }

  // Show tooltips.
  auto* tooltip_manager = quick_answers_view_->GetWidget()->GetTooltipManager();
  if (tooltip_manager)
    tooltip_manager->UpdateTooltip();
}

bool QuickAnswersPreTargetHandler::DoDispatchEvent(views::View* view,
                                                   ui::LocatedEvent* event) {
  DCHECK(view && event);

  // Out-of-bounds `ET_MOUSE_MOVED` events are allowed to sift through to
  // clear any set hover-state.
  // TODO (siabhijeet): Two-phased dispatching via widget should fix this.
  if (!view->HitTestPoint(event->location()) &&
      event->type() != ui::ET_MOUSE_MOVED) {
    return false;
  }

  // Post-order dispatch the event on child views in reverse Z-order.
  auto children = view->GetChildrenInZOrder();
  for (auto* child : base::Reversed(children)) {
    // Dispatch a fresh event to preserve the |event| for the parent target.
    std::unique_ptr<ui::Event> to_dispatch;
    if (event->IsMouseEvent()) {
      to_dispatch =
          std::make_unique<ui::MouseEvent>(*event->AsMouseEvent(), view, child);
    } else if (event->IsGestureEvent()) {
      to_dispatch = std::make_unique<ui::GestureEvent>(*event->AsGestureEvent(),
                                                       view, child);
    } else {
      return false;
    }
    ui::Event::DispatcherApi(to_dispatch.get()).set_target(child);
    if (DoDispatchEvent(child, to_dispatch.get()->AsLocatedEvent()))
      return true;
  }

  view->OnEvent(event);
  return event->handled();
}

}  // namespace ash
