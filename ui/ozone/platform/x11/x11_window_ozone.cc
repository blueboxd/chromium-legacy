// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate,
                               X11WindowManagerOzone* window_manager)
    : X11Window(delegate), window_manager_(window_manager) {
  DCHECK(window_manager_);
}

X11WindowOzone::~X11WindowOzone() {
  PrepareForShutdown();
  Close();
}

void X11WindowOzone::OnLostCapture() {
  X11Window::OnXWindowLostCapture();
}

void X11WindowOzone::Close() {
  if (is_shutting_down_)
    return;

  is_shutting_down_ = true;
  RemoveFromWindowManager();
  SetWidget(x11::None);

  X11Window::Close();
}

void X11WindowOzone::SetCapture() {
  if (HasCapture())
    return;

  X11Window::SetCapture();
  window_manager_->GrabEvents(this);
}

void X11WindowOzone::ReleaseCapture() {
  if (!HasCapture())
    return;

  X11Window::ReleasePointerGrab();
  window_manager_->UngrabEvents(this);
}

bool X11WindowOzone::HasCapture() const {
  return window_manager_->event_grabber() == this;
}
void X11WindowOzone::PrepareForShutdown() {
  DCHECK(X11EventSource::GetInstance());
  X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XWindow::SetCursor(cursor_ozone->xcursor());
}

void X11WindowOzone::OnMouseEnter() {
  platform_window_delegate()->OnMouseEnter();
}

void X11WindowOzone::RemoveFromWindowManager() {
  DCHECK(window_manager_);
  if (widget_ != gfx::kNullAcceleratedWidget) {
    window_manager_->RemoveWindow(this);
  }
}

// CheckCanDispatchNextPlatformEvent is called by X11EventSourceLibevent to
// determine whether X11WindowOzone instance (XEventDispatcher implementation)
// is able to process next translated event sent by it. So, it's done through
// |handle_next_event_| internal flag, used in subsequent CanDispatchEvent
// call.
void X11WindowOzone::CheckCanDispatchNextPlatformEvent(XEvent* xev) {
  if (is_shutting_down_)
    return;

  handle_next_event_ = XWindow::IsTargetedBy(*xev);
}

void X11WindowOzone::PlatformEventDispatchFinished() {
  handle_next_event_ = false;
}

PlatformEventDispatcher* X11WindowOzone::GetPlatformEventDispatcher() {
  return this;
}

bool X11WindowOzone::DispatchXEvent(XEvent* xev) {
  if (!XWindow::IsTargetedBy(*xev))
    return false;

  XWindow::ProcessEvent(xev);
  return true;
}

bool X11WindowOzone::CanDispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(XWindow::window(), x11::None);
  return handle_next_event_;
}

uint32_t X11WindowOzone::DispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(XWindow::window(), x11::None);

  if (event->IsMouseEvent() && handle_next_event_)
    window_manager_->MouseOnWindow(this);

  if (!window_manager_->event_grabber() ||
      window_manager_->event_grabber() == this) {
    // This is unfortunately needed otherwise events that depend on global state
    // (eg. double click) are broken.
    DispatchEventFromNativeUiEvent(
        event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                              base::Unretained(platform_window_delegate())));
    return POST_DISPATCH_STOP_PROPAGATION;
  }

  if (event->IsLocatedEvent()) {
    // Another X11WindowOzone has installed itself as capture. Translate the
    // event's location and dispatch to the other.
    ConvertEventLocationToTargetWindowLocation(
        window_manager_->event_grabber()->GetBounds().origin(),
        GetBounds().origin(), event->AsLocatedEvent());
  }
  return window_manager_->event_grabber()->DispatchEvent(event);
}

void X11WindowOzone::SetWidget(XID xid) {
  // In spite of being defined in Xlib as `unsigned long`, XID (|xid|'s type)
  // is fixed at 32-bits (CARD32) in X11 Protocol, therefore can't be larger
  // than 32 bits values on the wire (see https://crbug.com/607014 for more
  // details). So, It's safe to use static_cast here.
  widget_ = static_cast<gfx::AcceleratedWidget>(xid);
}

void X11WindowOzone::OnXWindowCreated() {
  DCHECK_NE(XWindow::window(), x11::None);
  SetWidget(XWindow::window());

  window_manager_->AddWindow(this);
  X11Window::OnXWindowCreated();
}

void X11WindowOzone::SetPlatformEventDispatcher() {
  DCHECK(X11EventSource::GetInstance());
  X11EventSource::GetInstance()->AddXEventDispatcher(this);
}

}  // namespace ui
