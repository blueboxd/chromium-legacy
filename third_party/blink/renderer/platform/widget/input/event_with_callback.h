// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_EVENT_WITH_CALLBACK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_EVENT_WITH_CALLBACK_H_

#include <list>

#include "third_party/blink/public/platform/input/input_handler_proxy.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/latency/latency_info.h"

namespace blink {

namespace test {
class InputHandlerProxyEventQueueTest;
}

class PLATFORM_EXPORT EventWithCallback {
 public:
  using WebScopedInputEvent = std::unique_ptr<WebInputEvent>;

  struct PLATFORM_EXPORT OriginalEventWithCallback {
    OriginalEventWithCallback(
        WebScopedInputEvent event,
        const ui::LatencyInfo& latency,
        InputHandlerProxy::EventDispositionCallback callback);
    ~OriginalEventWithCallback();
    WebScopedInputEvent event_;
    ui::LatencyInfo latency_;
    InputHandlerProxy::EventDispositionCallback callback_;
  };
  using OriginalEventList = std::list<OriginalEventWithCallback>;

  EventWithCallback(WebScopedInputEvent event,
                    const ui::LatencyInfo& latency,
                    base::TimeTicks timestamp_now,
                    InputHandlerProxy::EventDispositionCallback callback);
  EventWithCallback(WebScopedInputEvent event,
                    const ui::LatencyInfo& latency,
                    base::TimeTicks creation_timestamp,
                    base::TimeTicks last_coalesced_timestamp,
                    std::unique_ptr<OriginalEventList> original_events);
  ~EventWithCallback();

  bool CanCoalesceWith(const EventWithCallback& other) const WARN_UNUSED_RESULT;
  void CoalesceWith(EventWithCallback* other, base::TimeTicks timestamp_now);

  void RunCallbacks(InputHandlerProxy::EventDisposition,
                    const ui::LatencyInfo& latency,
                    std::unique_ptr<InputHandlerProxy::DidOverscrollParams>,
                    const WebInputEventAttribution&);

  const WebInputEvent& event() const { return *event_; }
  WebInputEvent* event_pointer() { return event_.get(); }
  const ui::LatencyInfo& latency_info() const { return latency_; }
  ui::LatencyInfo* mutable_latency_info() { return &latency_; }
  base::TimeTicks creation_timestamp() const { return creation_timestamp_; }
  base::TimeTicks last_coalesced_timestamp() const {
    return last_coalesced_timestamp_;
  }
  void set_coalesced_scroll_and_pinch() { coalesced_scroll_and_pinch_ = true; }
  bool coalesced_scroll_and_pinch() const {
    return coalesced_scroll_and_pinch_;
  }
  size_t coalesced_count() const { return original_events_.size(); }
  OriginalEventList& original_events() { return original_events_; }
  // |first_original_event()| is used as ID for tracing.
  WebInputEvent* first_original_event() {
    return original_events_.empty() ? nullptr
                                    : original_events_.front().event_.get();
  }
  void SetScrollbarManipulationHandledOnCompositorThread();

 private:
  friend class test::InputHandlerProxyEventQueueTest;

  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  WebScopedInputEvent event_;
  ui::LatencyInfo latency_;
  OriginalEventList original_events_;
  bool coalesced_scroll_and_pinch_ = false;

  base::TimeTicks creation_timestamp_;
  base::TimeTicks last_coalesced_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_EVENT_WITH_CALLBACK_H_
