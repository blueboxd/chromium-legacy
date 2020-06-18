// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source.h"

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xproto.h>

#include <algorithm>
#include <memory>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram_macros.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_hotplug_event_handler.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"

#if defined(USE_GLIB)
#include "ui/events/platform/x11/x11_event_watcher_glib.h"
#else
#include "ui/events/platform/x11/x11_event_watcher_fdwatch.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

namespace {

bool InitializeXkb(XDisplay* display) {
  if (!display)
    return false;

  int opcode, event, error;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
    DVLOG(1) << "Xkb extension not available.";
    return false;
  }

  // Ask the server not to send KeyRelease event when the user holds down a key.
  // crbug.com/138092
  x11::Bool supported_return;
  if (!XkbSetDetectableAutoRepeat(display, x11::True, &supported_return)) {
    DVLOG(1) << "XKB not supported in the server.";
    return false;
  }

  return true;
}

Time ExtractTimeFromXEvent(const x11::Event& x11_event) {
  const XEvent& xevent = x11_event.xlib_event();

  switch (xevent.type) {
    case x11::KeyEvent::Press:
    case x11::KeyEvent::Release:
      return xevent.xkey.time;
    case x11::ButtonEvent::Press:
    case x11::ButtonEvent::Release:
      return xevent.xbutton.time;
    case x11::MotionNotifyEvent::opcode:
      return xevent.xmotion.time;
    case x11::CrossingEvent::EnterNotify:
    case x11::CrossingEvent::LeaveNotify:
      return xevent.xcrossing.time;
    case x11::PropertyNotifyEvent::opcode:
      return xevent.xproperty.time;
    case x11::SelectionClearEvent::opcode:
      return xevent.xselectionclear.time;
    case x11::SelectionRequestEvent::opcode:
      return xevent.xselectionrequest.time;
    case x11::SelectionNotifyEvent::opcode:
      return xevent.xselection.time;
    case x11::GeGenericEvent::opcode:
      if (DeviceDataManagerX11::GetInstance()->IsXIDeviceEvent(x11_event))
        return static_cast<XIDeviceEvent*>(xevent.xcookie.data)->time;
      else
        break;
  }
  return x11::CurrentTime;
}

void UpdateDeviceList() {
  XDisplay* display = gfx::GetXDisplay();
  DeviceListCacheX11::GetInstance()->UpdateDeviceList(display);
  TouchFactory::GetInstance()->UpdateDeviceList(display);
  DeviceDataManagerX11::GetInstance()->UpdateDeviceList(display);
}

}  // namespace

#if defined(USE_GLIB)
using X11EventWatcherImpl = X11EventWatcherGlib;
#else
using X11EventWatcherImpl = X11EventWatcherFdWatch;
#endif

X11EventSource* X11EventSource::instance_ = nullptr;

X11EventSource::X11EventSource(XDisplay* display)
    : watcher_(std::make_unique<X11EventWatcherImpl>(this)),
      display_(display),
      dispatching_event_(nullptr),
      dummy_initialized_(false),
      distribution_(0, 999) {
  DCHECK(!instance_);
  instance_ = this;

  DCHECK(display_);
  DeviceDataManagerX11::CreateInstance();
  InitializeXkb(display_);

  watcher_->StartWatching();
}

X11EventSource::~X11EventSource() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
  if (dummy_initialized_)
    XDestroyWindow(display_, static_cast<uint32_t>(dummy_window_));
}

bool X11EventSource::HasInstance() {
  return instance_;
}

// static
X11EventSource* X11EventSource::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, public

void X11EventSource::DispatchXEvents() {
  continue_stream_ = true;
  x11::Connection::Get()->Dispatch(this);
}

Time X11EventSource::GetCurrentServerTime() {
  DCHECK(display_);

  if (!dummy_initialized_) {
    // Create a new Window and Atom that will be used for the property change.
    dummy_window_ = static_cast<x11::Window>(XCreateSimpleWindow(
        display_, DefaultRootWindow(display_), 0, 0, 1, 1, 0, 0, 0));
    dummy_atom_ = gfx::GetAtom("CHROMIUM_TIMESTAMP");
    dummy_window_events_ = std::make_unique<XScopedEventSelector>(
        dummy_window_, PropertyChangeMask);
    dummy_initialized_ = true;
  }

  // No need to measure Linux.X11.ServerRTT on every call.
  // base::TimeTicks::Now() itself has non-trivial overhead.
  bool measure_rtt = distribution_(generator_) == 0;

  base::TimeTicks start;
  if (measure_rtt)
    start = base::TimeTicks::Now();

  // Make a no-op property change on |dummy_window_|.
  auto* connection = x11::Connection::Get();
  connection->ChangeProperty({
      .window = static_cast<x11::Window>(dummy_window_),
      .property = dummy_atom_,
      .type = x11::Atom::STRING,
      .format = CHAR_BIT,
      .data_len = 1,
      .data = std::vector<uint8_t>{0},
  });

  // Observe the resulting PropertyNotify event to obtain the timestamp.
  connection->Sync();
  if (measure_rtt) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Linux.X11.ServerRTT",
        (base::TimeTicks::Now() - start).InMicroseconds(), 1,
        base::TimeDelta::FromMilliseconds(50).InMicroseconds(), 50);
  }
  connection->ReadResponses();

  Time time = x11::CurrentTime;
  auto pred = [&](const x11::Event& event) {
    if (event.xlib_event().type == x11::PropertyNotifyEvent::opcode &&
        event.xlib_event().xproperty.window ==
            static_cast<uint32_t>(dummy_window_)) {
      time = event.xlib_event().xproperty.time;
      return true;
    }
    return false;
  };

  auto& events = connection->events();
  events.erase(std::remove_if(events.begin(), events.end(), pred),
               events.end());
  return time;
}

Time X11EventSource::GetTimestamp() {
  if (dispatching_event_) {
    Time timestamp = ExtractTimeFromXEvent(*dispatching_event_);
    if (timestamp != x11::CurrentTime)
      return timestamp;
  }
  DVLOG(1) << "Making a round trip to get a recent server timestamp.";
  return GetCurrentServerTime();
}

base::Optional<gfx::Point>
X11EventSource::GetRootCursorLocationFromCurrentEvent() const {
  if (!dispatching_event_)
    return base::nullopt;

  DCHECK(dispatching_event_);
  XEvent* event = &dispatching_event_->xlib_event();

  bool is_xi2_event = event->type == x11::GeGenericEvent::opcode;
  int event_type = is_xi2_event
                       ? reinterpret_cast<XIDeviceEvent*>(event)->evtype
                       : event->type;

  bool is_valid_event = false;
  static_assert(XI_ButtonPress == x11::ButtonEvent::Press, "");
  static_assert(XI_ButtonRelease == x11::ButtonEvent::Release, "");
  static_assert(XI_Motion == x11::MotionNotifyEvent::opcode, "");
  static_assert(XI_Enter == x11::CrossingEvent::EnterNotify, "");
  static_assert(XI_Leave == x11::CrossingEvent::LeaveNotify, "");
  switch (event_type) {
    case x11::ButtonEvent::Press:
    case x11::ButtonEvent::Release:
    case x11::MotionNotifyEvent::opcode:
    case x11::CrossingEvent::EnterNotify:
    case x11::CrossingEvent::LeaveNotify:
      is_valid_event =
          is_xi2_event
              ? ui::TouchFactory::GetInstance()->ShouldProcessXI2Event(event)
              : true;
  }

  if (is_valid_event)
    return ui::EventSystemLocationFromXEvent(*dispatching_event_);
  return base::nullopt;
}

void X11EventSource::AddXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.AddObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    AddPlatformEventDispatcher(event_dispatcher);
}

void X11EventSource::RemoveXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.RemoveObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    RemovePlatformEventDispatcher(event_dispatcher);
}

void X11EventSource::AddXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void X11EventSource::RemoveXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScopedXEventDispatcher>
X11EventSource::OverrideXEventDispatcher(XEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return std::make_unique<ScopedXEventDispatcher>(&overridden_dispatcher_,
                                                  dispatcher);
}

void X11EventSource::RestoreOverridenXEventDispatcher() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

void X11EventSource::DispatchPlatformEvent(const PlatformEvent& event,
                                           x11::Event* xevent) {
  DCHECK(event);

  // First, tell the XEventDispatchers, which can have PlatformEventDispatcher,
  // an ui::Event is going to be sent next. It must make a promise to handle
  // next translated |event| sent by PlatformEventSource based on a XID in
  // |xevent| tested in CheckCanDispatchNextPlatformEvent(). This is needed
  // because it is not possible to access |event|'s associated NativeEvent* and
  // check if it is the event's target window (XID).
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.CheckCanDispatchNextPlatformEvent(xevent);

  DispatchEvent(event);

  // Explicitly reset a promise to handle next translated event.
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.PlatformEventDispatchFinished();
}

void X11EventSource::DispatchXEventToXEventDispatchers(x11::Event* xevent) {
  bool stop_dispatching = false;

  for (auto& observer : observers_)
    observer.WillProcessXEvent(xevent);

  if (overridden_dispatcher_) {
    stop_dispatching = overridden_dispatcher_->DispatchXEvent(xevent);
  }

  if (!stop_dispatching) {
    for (XEventDispatcher& dispatcher : dispatchers_xevent_) {
      if (dispatcher.DispatchXEvent(xevent))
        break;
    }
  }

  for (auto& observer : observers_)
    observer.DidProcessXEvent(xevent);

  // If an overridden dispatcher has been destroyed, then the event source
  // should halt dispatching the current stream of events, and wait until the
  // next message-loop iteration for dispatching events. This lets any nested
  // message-loop to unwind correctly and any new dispatchers to receive the
  // correct sequence of events.
  if (overridden_dispatcher_restored_)
    StopCurrentEventStream();

  overridden_dispatcher_restored_ = false;
}

void XEventDispatcher::CheckCanDispatchNextPlatformEvent(x11::Event* xev) {}

void XEventDispatcher::PlatformEventDispatchFinished() {}

PlatformEventDispatcher* XEventDispatcher::GetPlatformEventDispatcher() {
  return nullptr;
}

void X11EventSource::ProcessXEvent(x11::Event* xevent) {
  auto translated_event = ui::BuildEventFromXEvent(*xevent);
  // Ignore native platform-events only if they correspond to mouse events.
  // Allow other types of events to still be handled
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents() &&
      translated_event && translated_event->IsMouseEvent()) {
    return;
  }
  if (translated_event && translated_event->type() != ET_UNKNOWN) {
#if defined(OS_CHROMEOS)
    if (translated_event->IsLocatedEvent()) {
      ui::CursorController::GetInstance()->SetCursorLocation(
          translated_event->AsLocatedEvent()->location_f());
    }
#endif
    DispatchPlatformEvent(translated_event.get(), xevent);
  } else {
    // Only if we can't translate XEvent into ui::Event, try to dispatch XEvent
    // directly to XEventDispatchers.
    DispatchXEventToXEventDispatchers(xevent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// X11EventSource, protected

void X11EventSource::PostDispatchEvent(x11::Event* x11_event) {
  XEvent* xevent = &x11_event->xlib_event();
  bool should_update_device_list = false;

  if (xevent->type == x11::GeGenericEvent::opcode) {
    if (xevent->xgeneric.evtype == XI_HierarchyChanged) {
      should_update_device_list = true;
    } else if (xevent->xgeneric.evtype == XI_DeviceChanged) {
      XIDeviceChangedEvent* xev =
          static_cast<XIDeviceChangedEvent*>(xevent->xcookie.data);
      if (xev->reason == XIDeviceChange) {
        should_update_device_list = true;
      } else if (xev->reason == XISlaveSwitch) {
        ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses(
            xev->sourceid);
      }
    }
  }

  if (should_update_device_list) {
    UpdateDeviceList();
    hotplug_event_handler_->OnHotplugEvent();
  }

  if (xevent->type == x11::CrossingEvent::EnterNotify &&
      xevent->xcrossing.detail != NotifyInferior &&
      xevent->xcrossing.mode != NotifyUngrab) {
    // Clear stored scroll data
    ui::DeviceDataManagerX11::GetInstance()->InvalidateScrollClasses(
        DeviceDataManagerX11::kAllDevices);
  }
}

void X11EventSource::StopCurrentEventStream() {
  continue_stream_ = false;
}

void X11EventSource::OnDispatcherListChanged() {
  watcher_->StartWatching();

  if (!hotplug_event_handler_) {
    hotplug_event_handler_ = std::make_unique<X11HotplugEventHandler>();
    // Force the initial device query to have an update list of active devices.
    hotplug_event_handler_->OnHotplugEvent();
  }
}

bool X11EventSource::ShouldContinueStream() const {
  return continue_stream_;
}

void X11EventSource::DispatchXEvent(x11::Event* event) {
  // NB: The event should be reset to nullptr when this function
  // returns, not to its initial value, otherwise nested message loops
  // will incorrectly think that the current event being dispatched is
  // an old event.  This means base::AutoReset should not be used.
  dispatching_event_ = event;

  ProcessXEvent(event);
  PostDispatchEvent(event);

  dispatching_event_ = nullptr;
}

// ScopedXEventDispatcher implementation
ScopedXEventDispatcher::ScopedXEventDispatcher(
    XEventDispatcher** scoped_dispatcher,
    XEventDispatcher* new_dispatcher)
    : original_(*scoped_dispatcher),
      restore_(scoped_dispatcher, new_dispatcher) {}

ScopedXEventDispatcher::~ScopedXEventDispatcher() {
  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->RestoreOverridenXEventDispatcher();
}

// static
#if !defined(USE_OZONE)
std::unique_ptr<PlatformEventSource> PlatformEventSource::CreateDefault() {
  return std::make_unique<X11EventSource>(gfx::GetXDisplay());
}
#endif

}  // namespace ui
