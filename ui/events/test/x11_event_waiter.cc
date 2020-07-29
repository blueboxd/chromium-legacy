// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/x11_event_waiter.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// static
XEventWaiter* XEventWaiter::Create(x11::Window window,
                                   base::OnceClosure callback) {
  Display* display = gfx::GetXDisplay();
  static XEvent* marker_event = nullptr;
  if (!marker_event) {
    marker_event = new XEvent();
    marker_event->xclient.type = ClientMessage;
    marker_event->xclient.display = display;
    marker_event->xclient.window = static_cast<uint32_t>(window);
    marker_event->xclient.format = 8;
  }
  marker_event->xclient.message_type = static_cast<uint32_t>(MarkerEventAtom());

  XSendEvent(display, static_cast<uint32_t>(window), x11::False, 0,
             marker_event);
  XFlush(display);

  // Will be deallocated when the expected event is received.
  return new XEventWaiter(std::move(callback));
}

// XEventWaiter implementation
XEventWaiter::XEventWaiter(base::OnceClosure callback)
    : success_callback_(std::move(callback)) {
  ui::X11EventSource::GetInstance()->AddXEventObserver(this);
}

XEventWaiter::~XEventWaiter() {
  ui::X11EventSource::GetInstance()->RemoveXEventObserver(this);
}

void XEventWaiter::WillProcessXEvent(x11::Event* xev) {
  auto* client = xev->As<x11::ClientMessageEvent>();
  if (client && client->type == MarkerEventAtom()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(success_callback_));
    delete this;
  }
}

// Returns atom that indidates that the XEvent is marker event.
x11::Atom XEventWaiter::MarkerEventAtom() {
  return gfx::GetAtom("marker_event");
}

}  // namespace ui
