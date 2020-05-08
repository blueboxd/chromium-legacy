// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_coalesced_input_event.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace blink {

WebInputEvent* WebCoalescedInputEvent::EventPointer() {
  return event_.get();
}

void WebCoalescedInputEvent::AddCoalescedEvent(
    const blink::WebInputEvent& event) {
  coalesced_events_.emplace_back(event.Clone());
}

const WebInputEvent& WebCoalescedInputEvent::Event() const {
  return *event_.get();
}

size_t WebCoalescedInputEvent::CoalescedEventSize() const {
  return coalesced_events_.size();
}

const WebInputEvent& WebCoalescedInputEvent::CoalescedEvent(
    size_t index) const {
  return *coalesced_events_[index].get();
}

const std::vector<WebCoalescedInputEvent::WebScopedInputEvent>&
WebCoalescedInputEvent::GetCoalescedEventsPointers() const {
  return coalesced_events_;
}

void WebCoalescedInputEvent::AddPredictedEvent(
    const blink::WebInputEvent& event) {
  predicted_events_.emplace_back(event.Clone());
}

size_t WebCoalescedInputEvent::PredictedEventSize() const {
  return predicted_events_.size();
}

const WebInputEvent& WebCoalescedInputEvent::PredictedEvent(
    size_t index) const {
  return *predicted_events_[index].get();
}

const std::vector<WebCoalescedInputEvent::WebScopedInputEvent>&
WebCoalescedInputEvent::GetPredictedEventsPointers() const {
  return predicted_events_;
}

WebCoalescedInputEvent::WebCoalescedInputEvent(const WebInputEvent& event) {
  event_ = event.Clone();
  coalesced_events_.emplace_back(event.Clone());
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    std::unique_ptr<WebInputEvent> event,
    std::vector<std::unique_ptr<WebInputEvent>> coalesced_events,
    std::vector<std::unique_ptr<WebInputEvent>> predicted_events)
    : event_(std::move(event)),
      coalesced_events_(std::move(coalesced_events)),
      predicted_events_(std::move(predicted_events)) {}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebCoalescedInputEvent& event) {
  event_ = event.event_->Clone();
  for (const auto& coalesced_event : event.coalesced_events_)
    coalesced_events_.emplace_back(coalesced_event->Clone());
  for (const auto& predicted_event : event.predicted_events_)
    predicted_events_.emplace_back(predicted_event->Clone());
}

}  // namespace blink
