// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INPUT_WEB_COALESCED_INPUT_EVENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INPUT_WEB_COALESCED_INPUT_EVENT_H_

#include <memory>
#include <vector>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"

namespace blink {

// This class represents a polymorphic WebInputEvent structure with its
// coalesced events. The event could be any event defined in web_input_event.h,
// including those that cannot be coalesced.
class BLINK_COMMON_EXPORT WebCoalescedInputEvent {
 public:
  explicit WebCoalescedInputEvent(const WebInputEvent&);
  WebCoalescedInputEvent(std::unique_ptr<WebInputEvent>,
                         std::vector<std::unique_ptr<WebInputEvent>>,
                         std::vector<std::unique_ptr<WebInputEvent>>);
  // Copy constructor to deep copy the event.
  WebCoalescedInputEvent(const WebCoalescedInputEvent&);

  WebInputEvent* EventPointer();
  void AddCoalescedEvent(const blink::WebInputEvent&);
  const WebInputEvent& Event() const;
  size_t CoalescedEventSize() const;
  const WebInputEvent& CoalescedEvent(size_t index) const;
  const std::vector<std::unique_ptr<WebInputEvent>>&
  GetCoalescedEventsPointers() const;

  void AddPredictedEvent(const blink::WebInputEvent&);
  size_t PredictedEventSize() const;
  const WebInputEvent& PredictedEvent(size_t index) const;
  const std::vector<std::unique_ptr<WebInputEvent>>&
  GetPredictedEventsPointers() const;

 private:
  using WebScopedInputEvent = std::unique_ptr<WebInputEvent>;

  WebScopedInputEvent event_;
  std::vector<WebScopedInputEvent> coalesced_events_;
  std::vector<WebScopedInputEvent> predicted_events_;
};

using WebScopedCoalescedInputEvent = std::unique_ptr<WebCoalescedInputEvent>;

}  // namespace blink

#endif
