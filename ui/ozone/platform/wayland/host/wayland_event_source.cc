// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/message_loop/message_loop_current.h"
#include "ui/events/event.h"

namespace ui {

WaylandEventSource::WaylandEventSource(wl_display* display)
    : controller_(FROM_HERE), display_(display) {
  DCHECK(display_);
}

WaylandEventSource::~WaylandEventSource() {
  StopProcessingEvents();
}

bool WaylandEventSource::StartProcessingEvents() {
  DCHECK(display_);
  if (watching_)
    return true;

  DCHECK(display_);
  MaybePrepareReadQueue();
  wl_display_flush(display_);
  return StartWatchingFd(base::MessagePumpForUI::WATCH_READ);
}

bool WaylandEventSource::StopProcessingEvents() {
  if (!watching_)
    return false;

  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  watching_ = false;
  return controller_.StopWatchingFileDescriptor();
}

EventDispatchCallback WaylandEventSource::GetDispatchCallback() {
  return base::BindRepeating(&WaylandEventSource::ProcessEvent,
                             weak_ptr_factory_.GetWeakPtr());
}

void WaylandEventSource::OnFileCanReadWithoutBlocking(int fd) {
  if (prepared_) {
    prepared_ = false;
    if (wl_display_read_events(display_) == -1)
      return;
    wl_display_dispatch_pending(display_);
  }

  MaybePrepareReadQueue();

  if (!prepared_)
    return;

  // Automatic Flush.
  int ret = wl_display_flush(display_);
  if (ret != -1 || errno != EAGAIN)
    return;

  // if all data could not be written, errno will be set to EAGAIN and -1
  // returned. In that case, use poll on the display file descriptor to wait for
  // it to become writable again.
  StartWatchingFd(base::MessagePumpForUI::WATCH_WRITE);
}

void WaylandEventSource::OnFileCanWriteWithoutBlocking(int fd) {
  int ret = wl_display_flush(display_);
  if (ret != -1 || errno != EAGAIN)
    StartWatchingFd(base::MessagePumpForUI::WATCH_READ);
  else if (ret < 0 && errno != EPIPE && prepared_)
    wl_display_cancel_read(display_);

  // Otherwise just continue watching in the same mode.
}

void WaylandEventSource::OnDispatcherListChanged() {
  StartProcessingEvents();
}

bool WaylandEventSource::StartWatchingFd(
    base::WatchableIOMessagePumpPosix::Mode mode) {
  if (watching_) {
    // Stop watching first.
    watching_ = !controller_.StopWatchingFileDescriptor();
    DCHECK(!watching_);
  }

  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  int display_fd = wl_display_get_fd(display_);
  watching_ = base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
      display_fd, true, mode, &controller_, this);
  return watching_;
}

void WaylandEventSource::MaybePrepareReadQueue() {
  if (prepared_)
    return;

  if (wl_display_prepare_read(display_) != -1) {
    prepared_ = true;
    return;
  }
  // Nothing to read, send events to the queue.
  wl_display_dispatch_pending(display_);
}

void WaylandEventSource::ProcessEvent(Event* event) {
  DispatchEvent(event);
}

}  // namespace ui
