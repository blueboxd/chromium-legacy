// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <xcb/xcb.h>

// Xlibint.h defines those as macros, which breaks the C++ versions in
// the std namespace.
#undef max
#undef min

#include <algorithm>

#include "base/command_line.h"
#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/x11_switches.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

namespace {

// On the wire, sequence IDs are 16 bits.  In xcb, they're usually extended to
// 32 and sometimes 64 bits.  In Xlib, they're extended to unsigned long, which
// may be 32 or 64 bits depending on the platform.  This function is intended to
// prevent bugs caused by comparing two differently sized sequences.  Also
// handles rollover.  To use, compare the result of this function with 0.  For
// example, to compare seq1 <= seq2, use CompareSequenceIds(seq1, seq2) <= 0.
template <typename T, typename U>
auto CompareSequenceIds(T t, U u) {
  static_assert(std::is_unsigned<T>::value, "");
  static_assert(std::is_unsigned<U>::value, "");
  // Cast to the smaller of the two types so that comparisons will always work.
  // If we casted to the larger type, then the smaller type will be zero-padded
  // and may incorrectly compare less than the other value.
  using SmallerType =
      typename std::conditional<sizeof(T) <= sizeof(U), T, U>::type;
  SmallerType t0 = static_cast<SmallerType>(t);
  SmallerType u0 = static_cast<SmallerType>(u);
  using SignedType = typename std::make_signed<SmallerType>::type;
  return static_cast<SignedType>(t0 - u0);
}

XDisplay* OpenNewXDisplay() {
  if (!XInitThreads())
    return nullptr;
  std::string display_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kX11Display);
  return XOpenDisplay(display_str.empty() ? nullptr : display_str.c_str());
}

}  // namespace

Connection::Event::Event(base::Optional<uint32_t> sequence,
                         const XEvent& xlib_event)
    : sequence(sequence), xlib_event(xlib_event) {}

Connection::Event::Event(xcb_generic_event_t* xcb_event,
                         x11::Connection* connection) {
  XDisplay* display = connection->display();

  sequence = xcb_event->full_sequence;
  // KeymapNotify events are the only events that don't have a sequence.
  if ((xcb_event->response_type & 0x7f) != x11::KeymapNotifyEvent::opcode) {
    // Rewrite the sequence to the last seen sequence so that Xlib doesn't
    // think the sequence wrapped around.
    xcb_event->sequence = XLastKnownRequestProcessed(display);

    // On the wire, events are 32 bytes except for generic events which are
    // trailed by additional data.  XCB inserts an extended 4-byte sequence
    // between the 32-byte event and the additional data, so we need to shift
    // the additional data over by 4 bytes so the event is back in its wire
    // format, which is what Xlib and XProto are expecting.
    if ((xcb_event->response_type & 0x7f) == x11::GeGenericEvent::opcode) {
      auto* ge = reinterpret_cast<xcb_ge_event_t*>(xcb_event);
      memmove(&ge->full_sequence, &ge[1], ge->length * 4);
    }
  }

  _XEnq(display, reinterpret_cast<xEvent*>(xcb_event));
  XNextEvent(display, &xlib_event);
  if (xlib_event.type == x11::GeGenericEvent::opcode)
    XGetEventData(display, &xlib_event.xcookie);
}

Connection::Event::Event(Event&& event) {
  xlib_event = event.xlib_event;
  sequence = event.sequence;
  memset(&event, 0, sizeof(Event));
}

Connection::Event& Connection::Event::operator=(Event&& event) {
  xlib_event = event.xlib_event;
  sequence = event.sequence;
  memset(&event, 0, sizeof(Event));
  return *this;
}

Connection::Event::~Event() {
  if (xlib_event.type == x11::GeGenericEvent::opcode && xlib_event.xcookie.data)
    XFreeEventData(xlib_event.xcookie.display, &xlib_event.xcookie);
}

Connection* Connection::Get() {
  static Connection* instance = new Connection;
  return instance;
}

Connection::Connection() : XProto(this), display_(OpenNewXDisplay()) {
  if (!display_)
    return;

  XSetEventQueueOwner(display_, XCBOwnsEventQueue);

  setup_ = std::make_unique<Setup>(Read<Setup>(
      reinterpret_cast<const uint8_t*>(xcb_get_setup(XcbConnection()))));
  default_screen_ = &setup_->roots[DefaultScreen(display_)];
  default_root_depth_ = &*std::find_if(
      default_screen_->allowed_depths.begin(),
      default_screen_->allowed_depths.end(), [&](const Depth& depth) {
        return depth.depth == default_screen_->root_depth;
      });
  defualt_root_visual_ = &*std::find_if(
      default_root_depth_->visuals.begin(), default_root_depth_->visuals.end(),
      [&](const VisualType visual) {
        return visual.visual_id == default_screen_->root_visual;
      });

  ExtensionManager::Init(this);
  if (bigreq()) {
    if (auto response = bigreq()->Enable({}).Sync())
      extended_max_request_length_ = response->maximum_request_length;
  }
}

Connection::~Connection() {
  if (display_)
    XCloseDisplay(display_);
}

xcb_connection_t* Connection::XcbConnection() {
  if (!display())
    return nullptr;
  return XGetXCBConnection(display());
}

Connection::Request::Request(unsigned int sequence,
                             FutureBase::ResponseCallback callback)
    : sequence(sequence), callback(std::move(callback)) {}

Connection::Request::Request(Request&& other)
    : sequence(other.sequence), callback(std::move(other.callback)) {}

Connection::Request::~Request() = default;

bool Connection::HasNextResponse() const {
  return !requests_.empty() &&
         CompareSequenceIds(XLastKnownRequestProcessed(display_),
                            requests_.front().sequence) >= 0;
}

void Connection::Flush() {
  XFlush(display_);
}

void Connection::Sync() {
  GetInputFocus({}).Sync();
}

void Connection::ReadResponses() {
  while (auto* event = xcb_poll_for_event(XcbConnection())) {
    events_.emplace_back(event, this);
    free(event);
  }
}

bool Connection::HasPendingResponses() const {
  return !events_.empty() || HasNextResponse();
}

void Connection::Dispatch(Delegate* delegate) {
  DCHECK(display_);

  auto process_next_response = [&] {
    xcb_connection_t* connection = XGetXCBConnection(display_);
    auto request = std::move(requests_.front());
    requests_.pop();

    void* raw_reply = nullptr;
    xcb_generic_error_t* raw_error = nullptr;
    xcb_poll_for_reply(connection, request.sequence, &raw_reply, &raw_error);

    std::move(request.callback)
        .Run(FutureBase::RawReply{reinterpret_cast<uint8_t*>(raw_reply)},
             FutureBase::RawError{raw_error});
  };

  auto process_next_event = [&] {
    DCHECK(!events_.empty());

    Event event = std::move(events_.front());
    events_.pop_front();
    delegate->DispatchXEvent(&event.xlib_event);
  };

  // Handle all pending events.
  while (delegate->ShouldContinueStream()) {
    Flush();
    ReadResponses();

    if (HasNextResponse() && !events_.empty()) {
      if (!events_.front().sequence.has_value()) {
        process_next_event();
        continue;
      }

      auto next_response_sequence = requests_.front().sequence;
      auto next_event_sequence = events_.front().sequence.value();

      // All events have the sequence number of the last processed request
      // included in them.  So if a reply and an event have the same sequence,
      // the reply must have been received first.
      if (CompareSequenceIds(next_event_sequence, next_response_sequence) <= 0)
        process_next_response();
      else
        process_next_event();
    } else if (HasNextResponse()) {
      process_next_response();
    } else if (!events_.empty()) {
      process_next_event();
    } else {
      break;
    }
  }
}

void Connection::AddRequest(unsigned int sequence,
                            FutureBase::ResponseCallback callback) {
  DCHECK(requests_.empty() ||
         CompareSequenceIds(requests_.back().sequence, sequence) < 0);

  requests_.emplace(sequence, std::move(callback));
}

}  // namespace x11
