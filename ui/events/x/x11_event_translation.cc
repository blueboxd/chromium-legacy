// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

// In X11 touch events, a new tracking_id/slot mapping is set up for each new
// event (see |ui::GetTouchIdFromXEvent| function), which needs to be cleared
// at destruction time for corresponding release/cancel events. In this
// particular case, ui::TouchEvent class is extended so that dtor can be
// overridden in order to implement this platform-specific behavior.
class TouchEventX11 : public ui::TouchEvent {
 public:
  TouchEventX11(EventType type,
                gfx::Point location,
                base::TimeTicks timestamp,
                const PointerDetails& pointer_details)
      : TouchEvent(type, location, timestamp, pointer_details) {}

  ~TouchEventX11() override {
    if (type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED)
      TouchFactory::GetInstance()->ReleaseSlot(pointer_details().id);
  }
};

Event::Properties GetEventPropertiesFromXEvent(EventType type,
                                               const XEvent& xev) {
  using Values = std::vector<uint8_t>;
  Event::Properties properties;
  if (type == ET_KEY_PRESSED || type == ET_KEY_RELEASED) {
    // Keyboard group
    uint8_t group = XkbGroupForCoreState(xev.xkey.state);
    properties.emplace(kPropertyKeyboardGroup, Values{group});

    // Hardware keycode
    uint8_t hw_keycode = xev.xkey.keycode;
    properties.emplace(kPropertyKeyboardHwKeyCode, Values{hw_keycode});

    // IBus-gtk specific flags
    uint8_t ibus_flags = (xev.xkey.state >> kPropertyKeyboardIBusFlagOffset) &
                         kPropertyKeyboardIBusFlagMask;
    if (ibus_flags)
      properties.emplace(kPropertyKeyboardIBusFlag, Values{ibus_flags});

  } else if (type == ET_MOUSE_EXITED) {
    // NotifyVirtual events are created for intermediate windows that the
    // pointer crosses through. These occur when middle clicking.
    // Change these into mouse move events.
    bool crossing_intermediate_window = xev.xcrossing.detail == NotifyVirtual;
    if (crossing_intermediate_window) {
      properties.emplace(kPropertyMouseCrossedIntermediateWindow,
                         crossing_intermediate_window);
    }
  }
  return properties;
}

std::unique_ptr<KeyEvent> CreateKeyEvent(EventType event_type,
                                         const XEvent& xev) {
  KeyboardCode key_code = KeyboardCodeFromXKeyEvent(&xev);
  int event_flags = EventFlagsFromXEvent(xev);

  // In Ozone builds, keep DomCode/DomKey unset, so they are extracted lazily
  // in KeyEvent::ApplyLayout() which makes it possible for CrOS/Linux, for
  // example, to support host system keyboard layouts.
#if defined(USE_OZONE)
  auto event = std::make_unique<KeyEvent>(event_type, key_code, event_flags,
                                          EventTimeFromXEvent(xev));
#else
  auto event = std::make_unique<KeyEvent>(
      event_type, key_code, CodeFromXEvent(&xev), event_flags,
      GetDomKeyFromXEvent(&xev), EventTimeFromXEvent(xev));
#endif

  DCHECK(event);
  event->SetProperties(GetEventPropertiesFromXEvent(event_type, xev));
  event->InitializeNative();
  return event;
}

void SetEventSourceDeviceId(MouseEvent* event, const XEvent& xev) {
  DCHECK(event);
  if (xev.type == GenericEvent) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
    event->set_source_device_id(xiev->sourceid);
  }
}

std::unique_ptr<MouseEvent> CreateMouseEvent(EventType type,
                                             const XEvent& xev) {
  // Ignore EventNotify and LeaveNotify events from children of |xwindow_|.
  // NativeViewGLSurfaceGLX adds a child to |xwindow_|.
  // https://crbug.com/792322
  bool enter_or_leave = xev.type == EnterNotify || xev.type == LeaveNotify;
  if (enter_or_leave && xev.xcrossing.detail == NotifyInferior)
    return nullptr;

  PointerDetails details{EventPointerType::kMouse};
  auto event = std::make_unique<MouseEvent>(
      type, EventLocationFromXEvent(xev), EventSystemLocationFromXEvent(xev),
      EventTimeFromXEvent(xev), EventFlagsFromXEvent(xev),
      GetChangedMouseButtonFlagsFromXEvent(xev), details);

  DCHECK(event);
  SetEventSourceDeviceId(event.get(), xev);
  event->SetProperties(GetEventPropertiesFromXEvent(type, xev));
  event->InitializeNative();
  return event;
}

std::unique_ptr<MouseWheelEvent> CreateMouseWheelEvent(const XEvent& xev) {
  int button_flags = (xev.type == GenericEvent)
                         ? GetChangedMouseButtonFlagsFromXEvent(xev)
                         : 0;
  auto event = std::make_unique<MouseWheelEvent>(
      GetMouseWheelOffsetFromXEvent(xev), EventLocationFromXEvent(xev),
      EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      EventFlagsFromXEvent(xev), button_flags);

  DCHECK(event);
  event->InitializeNative();
  return event;
}

std::unique_ptr<TouchEvent> CreateTouchEvent(EventType type,
                                             const XEvent& xev) {
  auto event = std::make_unique<TouchEventX11>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      GetTouchPointerDetailsFromXEvent(xev));
#if defined(USE_OZONE)
  // Touch events don't usually have |root_location| set differently than
  // |location|, since there is a touch device to display association, but this
  // doesn't happen in Ozone X11.
  event->set_root_location(EventSystemLocationFromXEvent(xev));
#endif
  return event;
}

std::unique_ptr<ScrollEvent> CreateScrollEvent(EventType type,
                                               const XEvent& xev) {
  float x_offset, y_offset, x_offset_ordinal, y_offset_ordinal;
  int finger_count = 0;

  if (type == ET_SCROLL) {
    GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                               &y_offset_ordinal, &finger_count);
  } else {
    GetFlingDataFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                           &y_offset_ordinal, nullptr);
  }
  auto event = std::make_unique<ScrollEvent>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      EventFlagsFromXEvent(xev), x_offset, y_offset, x_offset_ordinal,
      y_offset_ordinal, finger_count);

  DCHECK(event);
  // We need to filter zero scroll offset here. Because MouseWheelEventQueue
  // assumes we'll never get a zero scroll offset event and we need delta to
  // determine which element to scroll on phaseBegan.
  return (event->x_offset() != 0.0 || event->y_offset() != 0.0)
             ? std::move(event)
             : nullptr;
}

// Translates XI2 XEvent into a ui::Event.
std::unique_ptr<ui::Event> TranslateFromXI2Event(const XEvent& xev,
                                                 EventType event_type) {
  switch (event_type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      return CreateKeyEvent(event_type, xev);
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
      return CreateMouseEvent(event_type, xev);
    case ET_MOUSEWHEEL:
      return CreateMouseWheelEvent(xev);
    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL:
      return CreateScrollEvent(event_type, xev);
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

std::unique_ptr<Event> TranslateFromXEvent(const XEvent& xev) {
  EventType event_type = EventTypeFromXEvent(xev);
  switch (xev.type) {
    case LeaveNotify:
    case EnterNotify:
    case MotionNotify:
      return CreateMouseEvent(event_type, xev);
    case KeyPress:
    case KeyRelease:
      return CreateKeyEvent(event_type, xev);
    case ButtonPress:
    case ButtonRelease: {
      switch (event_type) {
        case ET_MOUSEWHEEL:
          return CreateMouseWheelEvent(xev);
        case ET_MOUSE_PRESSED:
        case ET_MOUSE_RELEASED:
          return CreateMouseEvent(event_type, xev);
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
      return TranslateFromXI2Event(xev, event_type);
  }
  return nullptr;
}

}  // namespace

// Translates a XEvent into a ui::Event.
std::unique_ptr<Event> BuildEventFromXEvent(const XEvent& xev) {
  auto event = TranslateFromXEvent(xev);
  if (event)
    ui::ComputeEventLatencyOS(event.get());
  return event;
}

// Convenience function that translates XEvent into ui::KeyEvent
std::unique_ptr<KeyEvent> BuildKeyEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsKeyEvent())
    return nullptr;
  return std::unique_ptr<KeyEvent>{event.release()->AsKeyEvent()};
}

// Convenience function that translates XEvent into ui::MouseEvent
std::unique_ptr<MouseEvent> BuildMouseEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseEvent())
    return nullptr;
  return std::unique_ptr<MouseEvent>{event.release()->AsMouseEvent()};
}

// Convenience function that translates XEvent into ui::TouchEvent
std::unique_ptr<TouchEvent> BuildTouchEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsTouchEvent())
    return nullptr;
  return std::unique_ptr<TouchEvent>{event.release()->AsTouchEvent()};
}

// Convenience function that translates XEvent into ui::MouseWheelEvent
std::unique_ptr<MouseWheelEvent> BuildMouseWheelEventFromXEvent(
    const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseWheelEvent())
    return nullptr;
  return std::unique_ptr<MouseWheelEvent>{event.release()->AsMouseWheelEvent()};
}

}  // namespace ui
