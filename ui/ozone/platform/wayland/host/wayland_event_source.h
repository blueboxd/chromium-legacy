// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/platform/platform_event_source.h"

struct wl_display;

namespace ui {

// Wayland implementation of PlatformEventSource. Responsible for polling for
// events from wayland connection file descriptor, which triggers input device
// objects' callbacks so that they can translate raw input events into
// |ui::Event| instances and injeting them into PlatformEvent system.
//
// TODO(crbug.com/1072009): For now, each input device object integrate with
// this component through a |EventDispatchCallback| instances which as created
// via |GetDispatchCallback| function and injected at construction time. In the
// next iteration of this refactoring, this will be modified in order to have an
// cleaner and more centralized approach.
class WaylandEventSource : public PlatformEventSource,
                           public base::MessagePumpForUI::FdWatcher {
 public:
  explicit WaylandEventSource(wl_display* display);
  WaylandEventSource(const WaylandEventSource&) = delete;
  WaylandEventSource& operator=(const WaylandEventSource&) = delete;
  ~WaylandEventSource() override;

  // Starts polling for events from the wayland connection file descriptor.
  // This method assumes connection is already estabilished and input objects
  // are already bound and properly initialized.
  bool StartProcessingEvents();

  // Stops polling for events from input devices.
  bool StopProcessingEvents();

  // Creates a new |EventDispatchCallback| callback that can be passed to input
  // devices so that they can inject events into PlatformEvent system.
  EventDispatchCallback GetDispatchCallback();

 private:
  // base::MessagePumpForUI::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // PlatformEventSource
  void OnDispatcherListChanged() override;

  bool StartWatchingFd(base::WatchableIOMessagePumpPosix::Mode mode);
  void MaybePrepareReadQueue();
  void ProcessEvent(Event* event);

  base::MessagePumpForUI::FdWatchController controller_;

  wl_display* const display_;  // Owned by WaylandConnection.

  bool watching_ = false;
  bool prepared_ = false;

  base::WeakPtrFactory<WaylandEventSource> weak_ptr_factory_{this};
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
