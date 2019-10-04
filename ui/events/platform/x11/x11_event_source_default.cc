// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source_default.h"

#include <memory>

#include "base/message_loop/message_loop_current.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/x/x11.h"

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

namespace {

Event::Properties GetEventPropertiesFromXKeyEvent(const XKeyEvent& xev) {
  using Values = std::vector<uint8_t>;
  Event::Properties properties;

  // Keyboard group
  uint8_t group = XkbGroupForCoreState(xev.state);
  properties.emplace(kPropertyKeyboardGroup, Values{group});

  // IBus-gtk specific flags
  uint8_t ibus_flags = (xev.state >> kPropertyKeyboardIBusFlagOffset) &
                       kPropertyKeyboardIBusFlagMask;
  properties.emplace(kPropertyKeyboardIBusFlag, Values{ibus_flags});

  return properties;
}

std::unique_ptr<KeyEvent> CreateKeyEvent(const XEvent& xev) {
  // In CrOS/Linux builds, keep DomCode/DomKey unset in KeyEvent translation,
  // so they are extracted lazily in KeyEvent::ApplyLayout() which makes it
  // possible for CrOS to support host system keyboard layouts.
#if defined(OS_CHROMEOS)
  auto key_event = std::make_unique<KeyEvent>(EventTypeFromXEvent(xev),
                                              KeyboardCodeFromXKeyEvent(&xev),
                                              EventFlagsFromXEvent(xev));
#else
  base::TimeTicks timestamp = EventTimeFromXEvent(xev);
  ValidateEventTimeClock(&timestamp);
  auto key_event = std::make_unique<KeyEvent>(
      EventTypeFromXEvent(xev), KeyboardCodeFromXKeyEvent(&xev),
      CodeFromXEvent(&xev), EventFlagsFromXEvent(xev),
      GetDomKeyFromXEvent(&xev), timestamp);
#endif
  // Attach keyboard group to |key_event|'s properties
  key_event->SetProperties(GetEventPropertiesFromXKeyEvent(xev.xkey));
  return key_event;
}

std::unique_ptr<TouchEvent> CreateTouchEvent(EventType event_type,
                                             const XEvent& xev) {
  std::unique_ptr<TouchEvent> event = std::make_unique<TouchEvent>(
      event_type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      GetTouchPointerDetailsFromXEvent(xev));

  // Touch events don't usually have |root_location| set differently than
  // |location|, since there is a touch device to display association, but this
  // doesn't happen in Ozone X11.
  event->set_root_location(EventSystemLocationFromXEvent(xev));

  return event;
}

// Translates XI2 XEvent into a ui::Event.
std::unique_ptr<ui::Event> TranslateXI2EventToEvent(const XEvent& xev) {
  EventType event_type = EventTypeFromXEvent(xev);
  switch (event_type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED: {
      XEvent xkeyevent = {0};
      InitXKeyEventFromXIDeviceEvent(xev, &xkeyevent);
      return CreateKeyEvent(xkeyevent);
    }
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED:
      return std::make_unique<MouseEvent>(
          event_type, EventLocationFromXEvent(xev),
          EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
          EventFlagsFromXEvent(xev), GetChangedMouseButtonFlagsFromXEvent(xev));
    case ET_MOUSEWHEEL:
      return std::make_unique<MouseWheelEvent>(
          GetMouseWheelOffsetFromXEvent(xev), EventLocationFromXEvent(xev),
          EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
          EventFlagsFromXEvent(xev), GetChangedMouseButtonFlagsFromXEvent(xev));
    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL: {
      float x_offset, y_offset, x_offset_ordinal, y_offset_ordinal;
      GetFlingDataFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                             &y_offset_ordinal, nullptr);
      return std::make_unique<ScrollEvent>(
          event_type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
          EventFlagsFromXEvent(xev), x_offset, y_offset, x_offset_ordinal,
          y_offset_ordinal, 0);
    }
    case ET_SCROLL: {
      float x_offset, y_offset, x_offset_ordinal, y_offset_ordinal;
      int finger_count;
      GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                                 &y_offset_ordinal, &finger_count);
      return std::make_unique<ScrollEvent>(
          event_type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
          EventFlagsFromXEvent(xev), x_offset, y_offset, x_offset_ordinal,
          y_offset_ordinal, finger_count);
    }
    case ET_TOUCH_MOVED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_CANCELLED:
    case ET_TOUCH_RELEASED:
      return CreateTouchEvent(event_type, xev);
    case ET_UNKNOWN:
      return nullptr;
    default:
      break;
  }
  return nullptr;
}

// Translates a XEvent into a ui::Event.
std::unique_ptr<ui::Event> TranslateXEventToEvent(const XEvent& xev) {
  int flags = EventFlagsFromXEvent(xev);
  switch (xev.type) {
    case LeaveNotify:
    case EnterNotify:
      // Don't generate synthetic mouse move events for EnterNotify/LeaveNotify
      // from nested XWindows. https://crbug.com/792322
      if (xev.xcrossing.detail == NotifyInferior)
        return nullptr;

      return std::make_unique<MouseEvent>(EventTypeFromXEvent(xev),
                                          EventLocationFromXEvent(xev),
                                          EventSystemLocationFromXEvent(xev),
                                          EventTimeFromXEvent(xev), flags, 0);

    case KeyPress:
    case KeyRelease:
      return CreateKeyEvent(xev);
    case ButtonPress:
    case ButtonRelease: {
      switch (EventTypeFromXEvent(xev)) {
        case ET_MOUSEWHEEL:
          return std::make_unique<MouseWheelEvent>(
              GetMouseWheelOffsetFromXEvent(xev), EventLocationFromXEvent(xev),
              EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
              flags, 0);
        case ET_MOUSE_PRESSED:
        case ET_MOUSE_RELEASED:
          return std::make_unique<MouseEvent>(
              EventTypeFromXEvent(xev), EventLocationFromXEvent(xev),
              EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
              flags, GetChangedMouseButtonFlagsFromXEvent(xev));
        case ET_UNKNOWN:
          // No event is created for X11-release events for mouse-wheel
          // buttons.
          break;
        default:
          NOTREACHED();
      }
      break;
    }

    case GenericEvent:
      return TranslateXI2EventToEvent(xev);
  }
  return nullptr;
}

}  // namespace

X11EventSourceDefault::X11EventSourceDefault(XDisplay* display)
    : event_source_(this, display), watcher_controller_(FROM_HERE) {
  AddEventWatcher();
}

X11EventSourceDefault::~X11EventSourceDefault() {}

// static
X11EventSourceDefault* X11EventSourceDefault::GetInstance() {
  return static_cast<X11EventSourceDefault*>(
      PlatformEventSource::GetInstance());
}

void X11EventSourceDefault::AddXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.AddObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    AddPlatformEventDispatcher(event_dispatcher);
}

void X11EventSourceDefault::RemoveXEventDispatcher(
    XEventDispatcher* dispatcher) {
  dispatchers_xevent_.RemoveObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    RemovePlatformEventDispatcher(event_dispatcher);
}

void X11EventSourceDefault::AddXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void X11EventSourceDefault::RemoveXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScopedXEventDispatcher>
X11EventSourceDefault::OverrideXEventDispatcher(XEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return std::make_unique<ScopedXEventDispatcher>(&overridden_dispatcher_,
                                                  dispatcher);
}

void X11EventSourceDefault::RestoreOverridenXEventDispatcher() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

void X11EventSourceDefault::ProcessXEvent(XEvent* xevent) {
  std::unique_ptr<ui::Event> translated_event = TranslateXEventToEvent(*xevent);
  if (translated_event) {
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

void X11EventSourceDefault::AddEventWatcher() {
  if (initialized_)
    return;
  if (!base::MessageLoopCurrent::Get())
    return;

  int fd = ConnectionNumber(event_source_.display());
  base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
      fd, true, base::MessagePumpForUI::WATCH_READ, &watcher_controller_, this);
  initialized_ = true;
}

void X11EventSourceDefault::DispatchPlatformEvent(const PlatformEvent& event,
                                                  XEvent* xevent) {
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

void X11EventSourceDefault::DispatchXEventToXEventDispatchers(XEvent* xevent) {
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

void X11EventSourceDefault::StopCurrentEventStream() {
  event_source_.StopCurrentEventStream();
}

void X11EventSourceDefault::OnDispatcherListChanged() {
  AddEventWatcher();
  event_source_.OnDispatcherListChanged();
}

void X11EventSourceDefault::OnFileCanReadWithoutBlocking(int fd) {
  event_source_.DispatchXEvents();
}

void X11EventSourceDefault::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void XEventDispatcher::CheckCanDispatchNextPlatformEvent(XEvent* xev) {}

void XEventDispatcher::PlatformEventDispatchFinished() {}

PlatformEventDispatcher* XEventDispatcher::GetPlatformEventDispatcher() {
  return nullptr;
}

}  // namespace ui
