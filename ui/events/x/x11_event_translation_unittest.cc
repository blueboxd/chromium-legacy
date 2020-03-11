// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/x/x11.h"

namespace ui {

// Ensure DomKey extraction happens lazily in Ozone X11, while in non-Ozone
// path it is set right away in XEvent => ui::Event translation. This prevents
// regressions such as crbug.com/1007389.
TEST(XEventTranslationTest, KeyEventDomKeyExtraction) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event xev;
  xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  KeyEventTestApi test(keyev.get());
#if defined(USE_OZONE)
  EXPECT_EQ(ui::DomKey::NONE, test.dom_key());
#else
  EXPECT_EQ(ui::DomKey::ENTER, test.dom_key());
#endif

  EXPECT_EQ(13, keyev->GetCharacter());
  EXPECT_EQ("Enter", keyev->GetCodeString());
}

// Ensure KeyEvent::Properties is properly set regardless X11 build config is
// in place. This prevents regressions such as crbug.com/1047999.
TEST(XEventTranslationTest, KeyEventXEventPropertiesSet) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event scoped_xev;
  scoped_xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_A, EF_NONE);

  XEvent* xev = scoped_xev;
  XDisplay* xdisplay = xev->xkey.display;
  // Set keyboard group in XKeyEvent
  xev->xkey.state = XkbBuildCoreState(xev->xkey.state, 2u);
  // Set IBus-specific flags
  xev->xkey.state |= 0x3 << ui::kPropertyKeyboardIBusFlagOffset;

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  auto* properties = keyev->properties();
  EXPECT_TRUE(properties);
  EXPECT_EQ(3u, properties->size());

  // Ensure hardware keycode, keyboard group and ibus flag properties are
  // properly set.
  auto hw_keycode_it = properties->find(ui::kPropertyKeyboardHwKeyCode);
  EXPECT_NE(hw_keycode_it, properties->end());
  EXPECT_EQ(1u, hw_keycode_it->second.size());
  EXPECT_EQ(XKeysymToKeycode(xdisplay, XK_a), hw_keycode_it->second[0]);

  auto kbd_group_it = properties->find(ui::kPropertyKeyboardGroup);
  EXPECT_NE(kbd_group_it, properties->end());
  EXPECT_EQ(1u, kbd_group_it->second.size());
  EXPECT_EQ(2u, kbd_group_it->second[0]);

  auto ibus_flag_it = properties->find(ui::kPropertyKeyboardIBusFlag);
  EXPECT_NE(ibus_flag_it, properties->end());
  EXPECT_EQ(1u, ibus_flag_it->second.size());
  EXPECT_EQ(0x3, ibus_flag_it->second[0]);
}

// Ensure XEvents with bogus timestamps are properly handled when translated
// into ui::*Events.
TEST(XEventTranslationTest, BogusTimestampCorrection) {
  using base::TimeDelta;
  using base::TimeTicks;

  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event scoped_xev;
  scoped_xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);
  XEvent* xev = scoped_xev;

  test::ScopedEventTestTickClock test_clock;
  test_clock.Advance(TimeDelta::FromSeconds(1));

  // Set initial time as 1000ms
  TimeTicks now_ticks = EventTimeForNow();
  int64_t now_ms = (now_ticks - TimeTicks()).InMilliseconds();
  EXPECT_EQ(1000, now_ms);

  // Emulate XEvent generated 500ms before current time (non-bogus) and verify
  // the translated Event uses native event's timestamp.
  xev->xkey.time = 500;
  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);
  EXPECT_EQ(now_ticks - TimeDelta::FromMilliseconds(500), keyev->time_stamp());

  // Emulate XEvent generated 1000ms ahead in time (bogus timestamp) and verify
  // the translated Event's timestamp is fixed using (i.e: EventTimeForNow()
  // instead of the original XEvent's time)
  xev->xkey.time = 2000;
  auto keyev2 = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev2);
  EXPECT_EQ(EventTimeForNow(), keyev2->time_stamp());

  // Emulate XEvent >= 60sec old (bogus timestamp) and ensure translated
  // ui::Event's timestamp has been corrected (i.e: use ui::EventTimeForNow()
  // instead of the original XEvent's time). To emulate such scenario, we
  // advance the clock by 5 minutes and set the XEvent's time to 1min, so delta
  // is 4min 1sec.
  test_clock.Advance(TimeDelta::FromMinutes(5));
  xev->xkey.time = 1000 * 60;
  auto keyev3 = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev3);
  EXPECT_EQ(EventTimeForNow(), keyev3->time_stamp());
}

// Ensure MouseEvent::changed_button_flags is correctly translated from
// X{Button,Crossing}Events.
TEST(XEventTranslationTest, ChangedMouseButtonFlags) {
  ui::ScopedXI2Event event;
  // Taking in a ButtonPress XEvent, with left button pressed.
  event.InitButtonEvent(ui::ET_MOUSE_PRESSED, gfx::Point(500, 500),
                        ui::EF_LEFT_MOUSE_BUTTON);
  auto mouseev = ui::BuildMouseEventFromXEvent(*event);
  EXPECT_TRUE(mouseev);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, mouseev->changed_button_flags());

  // Taking in a ButtonPress XEvent, with no button pressed.
  static_cast<XEvent*>(event)->xbutton.button = 0;
  auto mouseev2 = ui::BuildMouseEventFromXEvent(*event);
  EXPECT_TRUE(mouseev2);
  EXPECT_EQ(0, mouseev2->changed_button_flags());

  // Taking in a EnterNotify XEvent
  auto enter_event = std::make_unique<XEvent>();
  memset(enter_event.get(), 0, sizeof(XEvent));
  enter_event->type = EnterNotify;
  enter_event->xcrossing.detail = NotifyVirtual;
  auto mouseev3 = ui::BuildMouseEventFromXEvent(*enter_event);
  EXPECT_TRUE(mouseev3);
  EXPECT_EQ(0, mouseev3->changed_button_flags());
}

}  // namespace ui
