// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_
#define CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/types/scroll_types.h"

namespace cc {
struct ElementId;
struct OverscrollBehavior;
}

namespace ui {
class LatencyInfo;
}

namespace viz {
class FrameSinkId;
}

namespace content {

class RenderWidget;
class RenderWidgetInputHandlerDelegate;

// RenderWidgetInputHandler is an IPC-agnostic input handling class.
// IPC transport code should live in RenderWidget or RenderWidgetMusConnection.
class CONTENT_EXPORT RenderWidgetInputHandler {
 public:
  RenderWidgetInputHandler(RenderWidgetInputHandlerDelegate* delegate,
                           RenderWidget* widget);
  virtual ~RenderWidgetInputHandler();

  // Hit test the given point to find out the frame underneath and
  // returns the FrameSinkId for that frame. |local_point| returns the point
  // in the coordinate space of the FrameSinkId that was hit.
  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  // Handle input events from the input event provider.
  virtual void HandleInputEvent(
      const blink::WebCoalescedInputEvent& coalesced_event,
      const ui::LatencyInfo& latency_info,
      HandledEventCallback callback);

  // Handle overscroll from Blink.
  void DidOverscrollFromBlink(const gfx::Vector2dF& overscrollDelta,
                              const gfx::Vector2dF& accumulatedOverscroll,
                              const gfx::PointF& position,
                              const gfx::Vector2dF& velocity,
                              const cc::OverscrollBehavior& behavior);

  void InjectGestureScrollEvent(blink::WebGestureDevice device,
                                const gfx::Vector2dF& delta,
                                ui::ScrollGranularity granularity,
                                cc::ElementId scrollable_area_element_id,
                                blink::WebInputEvent::Type injected_type);

  bool handling_input_event() const { return handling_input_event_; }
  void set_handling_input_event(bool handling_input_event) {
    handling_input_event_ = handling_input_event;
  }

  // Process the touch action, returning whether the action should be relayed
  // to the browser.
  bool ProcessTouchAction(cc::TouchAction touch_action);

  // Process the new cursor and returns true if it has changed from the last
  // cursor.
  bool DidChangeCursor(const WebCursor& cursor);

  // Do a hit test for a given point in viewport coordinate.
  blink::WebHitTestResult GetHitTestResultAtPoint(const gfx::PointF& point);

 private:
  class HandlingState;
  struct InjectScrollGestureParams {
    blink::WebGestureDevice device;
    gfx::Vector2dF scroll_delta;
    ui::ScrollGranularity granularity;
    cc::ElementId scrollable_area_element_id;
    blink::WebInputEvent::Type type;
  };

  blink::WebInputEventResult HandleTouchEvent(
      const blink::WebCoalescedInputEvent& coalesced_event);

  void HandleInjectedScrollGestures(
      std::vector<InjectScrollGestureParams> injected_scroll_params,
      const WebInputEvent& input_event,
      const ui::LatencyInfo& original_latency_info);

  RenderWidgetInputHandlerDelegate* const delegate_;

  RenderWidget* const widget_;

  // Are we currently handling an input event?
  bool handling_input_event_ = false;

  // Current state from HandleInputEvent. This variable is stack allocated
  // and is not owned.
  HandlingState* handling_input_state_ = nullptr;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  base::Optional<WebCursor> current_cursor_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_ = false;

  // Whether the last injected scroll gesture was a GestureScrollBegin. Used to
  // determine which GestureScrollUpdate is the first in a gesture sequence for
  // latency classification.
  bool last_injected_gesture_was_begin_ = false;

  base::WeakPtrFactory<RenderWidgetInputHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetInputHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_
