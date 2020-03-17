// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_

#include "third_party/blink/public/platform/web_input_event_result.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
class WebCoalescedInputEvent;

// The interface from blink to Widgets with implementations outside of blink.
class WebExternalWidgetClient {
 public:
  virtual ~WebExternalWidgetClient() = default;

  // Called when the associated WebExternalWidget receives input and
  // needs the implementation to handle it.
  virtual WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent&) = 0;

  // Called when the associated WebExternalWidget wishes to dispatch
  // any pending buffered touch events. The implementation may choose to buffer
  // individual pointer events (received via HandleInputEvent) and dispatch
  // a single touch event indicating the changes since the last touch event.
  // This method is typically invoked once per frame whereas HandleInputEvent
  // may be invoked many times per frame (i.e. multiple fingers on the touch
  // surface).
  virtual WebInputEventResult DispatchBufferedTouchEvents() = 0;

  // Called when the associated WebExternalWidget has adjusted its size.
  virtual void DidResize(const gfx::Size& size) = 0;

  // Record the time it took for the first paint after the widget transitioned
  // from background inactive to active.
  virtual void RecordTimeToFirstActivePaint(base::TimeDelta duration) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_EXTERNAL_WIDGET_CLIENT_H_
