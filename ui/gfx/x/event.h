// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_EVENT_H_
#define UI_GFX_X_EVENT_H_

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <xcb/xcb.h>

#include <cstdint>
#include <utility>

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;
class Event;
struct ReadBuffer;

COMPONENT_EXPORT(X11)
void ReadEvent(Event* event, Connection* connection, ReadBuffer* buffer);

class COMPONENT_EXPORT(X11) Event {
 public:
  // Used to create events for testing.
  template <typename T>
  Event(XEvent* xlib_event, T&& xproto_event) {
    sequence_valid_ = true;
    sequence_ = xlib_event_.xany.serial;
    custom_allocated_xlib_event_ = true;
    xlib_event_ = *xlib_event;
    type_id_ = T::type_id;
    deleter_ = [](void* event) { delete reinterpret_cast<T*>(event); };
    T* event = new T(std::forward<T>(xproto_event));
    event_ = event;
    window_ = event->GetWindow();
  }

  Event();
  Event(xcb_generic_event_t* xcb_event,
        Connection* connection,
        bool sequence_valid = true);
  // |event_bytes| is modified and will not be valid after this call.
  // A copy is necessary if the original data is still needed.
  Event(scoped_refptr<base::RefCountedMemory> event_bytes,
        Connection* connection,
        bool sequence_valid = true);

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  Event(Event&& event);
  Event& operator=(Event&& event);

  ~Event();

  template <typename T>
  T* As() {
    if (type_id_ == T::type_id)
      return reinterpret_cast<T*>(event_);
    return nullptr;
  }

  template <typename T>
  const T* As() const {
    return const_cast<Event*>(this)->As<T>();
  }

  bool sequence_valid() const { return sequence_valid_; }
  uint32_t sequence() const { return sequence_; }

  const XEvent& xlib_event() const { return xlib_event_; }
  XEvent& xlib_event() { return xlib_event_; }

  x11::Window window() const { return window_ ? *window_ : x11::Window::None; }

 private:
  friend void ReadEvent(Event* event,
                        Connection* connection,
                        ReadBuffer* buffer);

  void Dealloc();

  bool sequence_valid_ = false;
  uint32_t sequence_ = 0;

  // Indicates if |xlib_event_| was allocated manually and therefore
  // needs to be freed manually.
  bool custom_allocated_xlib_event_ = false;
  XEvent xlib_event_{};

  // XProto event state.
  int type_id_ = 0;
  void (*deleter_)(void*) = nullptr;
  void* event_ = nullptr;

  // This member points to a field in |event_|, or may be nullptr if there's no
  // associated window for the event.  It's owned by |event_|, not us.
  x11::Window* window_ = nullptr;
};

}  // namespace x11

#endif  // UI_GFX_X_EVENT_H_
