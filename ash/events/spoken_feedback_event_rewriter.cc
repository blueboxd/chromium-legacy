// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/spoken_feedback_event_rewriter.h"

#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/spoken_feedback_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace ash {

SpokenFeedbackEventRewriter::SpokenFeedbackEventRewriter() = default;
SpokenFeedbackEventRewriter::~SpokenFeedbackEventRewriter() = default;

void SpokenFeedbackEventRewriter::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  DCHECK(event->IsKeyEvent()) << "Unexpected unhandled event type";
  // Send the event to the most recently rewritten event's continuation,
  // (that is, through its EventSource). Under the assumption that a single
  // SpokenFeedbackEventRewriter is not registered to multiple EventSources,
  // this will be the same as this event's original source.
  const char* failure_reason = nullptr;
  if (continuation_) {
    ui::EventDispatchDetails details = SendEvent(continuation_, event.get());
    if (details.dispatcher_destroyed)
      failure_reason = "destroyed dispatcher";
    else if (details.target_destroyed)
      failure_reason = "destroyed target";
  } else if (continuation_.WasInvalidated()) {
    failure_reason = "destroyed source";
  } else {
    failure_reason = "no prior rewrite";
  }
  if (failure_reason) {
    VLOG(0) << "Undispatched key " << event->AsKeyEvent()->key_code()
            << " due to " << failure_reason << ".";
  }
}

ui::EventDispatchDetails SpokenFeedbackEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  // Save continuation for |OnUnhandledSpokenFeedbackEvent()|.
  continuation_ = continuation;

  if (!delegate_ ||
      !Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    return SendEvent(continuation, &event);

  if (event.IsKeyEvent()) {
    const ui::KeyEvent* key_event = event.AsKeyEvent();

    bool capture = capture_all_keys_;

    // Always capture the Search key.
    capture |= key_event->IsCommandDown();

    // Don't capture tab as it gets consumed by Blink so never comes back
    // unhandled. In third_party/WebKit/Source/core/input/EventHandler.cpp, a
    // default tab handler consumes tab even when no focusable nodes are found;
    // it sets focus to Chrome and eats the event.
    if (key_event->GetDomKey() == ui::DomKey::TAB)
      capture = false;

    delegate_->DispatchKeyEventToChromeVox(ui::Event::Clone(event), capture);
    return capture ? DiscardEvent(continuation)
                   : SendEvent(continuation, &event);
  }

  if (send_mouse_events_ && event.IsMouseEvent())
    delegate_->DispatchMouseEventToChromeVox(ui::Event::Clone(event));

  return SendEvent(continuation, &event);
}

}  // namespace ash
