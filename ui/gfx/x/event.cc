// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/event.h"

#include <X11/Xlibint.h>
#include <X11/extensions/XInput2.h>

// Xlibint.h defines those as macros, which breaks the C++ versions in
// the std namespace.
#undef max
#undef min

#include <cstring>

#include "ui/gfx/x/connection.h"

namespace x11 {

Event::Event() = default;

Event::Event(xcb_generic_event_t* xcb_event,
             x11::Connection* connection,
             bool sequence_valid) {
  XDisplay* display = connection->display();

  sequence_valid_ = sequence_valid;
  sequence_ = xcb_event->full_sequence;
  // KeymapNotify events are the only events that don't have a sequence.
  if ((xcb_event->response_type & ~kSendEventMask) !=
      x11::KeymapNotifyEvent::opcode) {
    // Rewrite the sequence to the last seen sequence so that Xlib doesn't
    // think the sequence wrapped around.
    xcb_event->sequence = XLastKnownRequestProcessed(display);

    // On the wire, events are 32 bytes except for generic events which are
    // trailed by additional data.  XCB inserts an extended 4-byte sequence
    // between the 32-byte event and the additional data, so we need to shift
    // the additional data over by 4 bytes so the event is back in its wire
    // format, which is what Xlib and XProto are expecting.
    if ((xcb_event->response_type & ~kSendEventMask) ==
        x11::GeGenericEvent::opcode) {
      auto* ge = reinterpret_cast<xcb_ge_event_t*>(xcb_event);
      memmove(&ge->full_sequence, &ge[1], ge->length * 4);
    }
  }

  // Xlib sometimes modifies |xcb_event|, so let it handle the event after
  // we parse it with ReadEvent().
  ReadEvent(this, connection, reinterpret_cast<uint8_t*>(xcb_event));

  _XEnq(display, reinterpret_cast<xEvent*>(xcb_event));
  if (!XEventsQueued(display, QueuedAlready)) {
    // If Xlib gets an event it doesn't recognize (eg. from an
    // extension it doesn't know about), it won't add the event to the
    // queue.  In this case, zero-out the event data.  This will set
    // the event type to 0, which does not correspond to any event.
    // This is safe because event handlers should always check the
    // event type before downcasting to a concrete event.
    memset(&xlib_event_, 0, sizeof(xlib_event_));
    return;
  }
  XNextEvent(display, &xlib_event_);
  if (xlib_event_.type == x11::GeGenericEvent::opcode)
    XGetEventData(display, &xlib_event_.xcookie);
}

Event::Event(Event&& event) {
  memcpy(this, &event, sizeof(Event));
  memset(&event, 0, sizeof(Event));
}

Event& Event::operator=(Event&& event) {
  Dealloc();
  memcpy(this, &event, sizeof(Event));
  memset(&event, 0, sizeof(Event));
  return *this;
}

Event::~Event() {
  Dealloc();
}

void Event::Dealloc() {
  if (xlib_event_.type == x11::GeGenericEvent::opcode &&
      xlib_event_.xcookie.data) {
    if (custom_allocated_xlib_event_) {
      XIDeviceEvent* xiev =
          static_cast<XIDeviceEvent*>(xlib_event_.xcookie.data);
      delete[] xiev->valuators.mask;
      delete[] xiev->valuators.values;
      delete[] xiev->buttons.mask;
      delete xiev;
    } else {
      XFreeEventData(xlib_event_.xcookie.display, &xlib_event_.xcookie);
    }
  }
  if (deleter_)
    deleter_(event_);
}

}  // namespace x11
