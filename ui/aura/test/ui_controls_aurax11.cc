// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/ui_controls_aurax11.h"

namespace aura {
namespace test {
namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UIControlsAura;
using ui_controls::UP;

// Mask of the buttons currently down.
unsigned button_down_mask = 0;

}  // namespace

UIControlsX11::UIControlsX11(WindowTreeHost* host) : host_(host) {}

UIControlsX11::~UIControlsX11() = default;

bool UIControlsX11::SendKeyPress(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command) {
  return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                    base::OnceClosure());
}

bool UIControlsX11::SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                               ui::KeyboardCode key,
                                               bool control,
                                               bool shift,
                                               bool alt,
                                               bool command,
                                               base::OnceClosure closure) {
  XEvent xevent;
  xevent.xkey = {};
  xevent.xkey.type = x11::KeyEvent::Press;
  if (control)
    SetKeycodeAndSendThenMask(&xevent, XK_Control_L, ControlMask);
  if (shift)
    SetKeycodeAndSendThenMask(&xevent, XK_Shift_L, ShiftMask);
  if (alt)
    SetKeycodeAndSendThenMask(&xevent, XK_Alt_L, Mod1Mask);
  if (command)
    SetKeycodeAndSendThenMask(&xevent, XK_Meta_L, Mod4Mask);
  xevent.xkey.keycode = XKeysymToKeycode(
      gfx::GetXDisplay(), ui::XKeysymForWindowsKeyCode(key, shift));
  PostEventToWindowTreeHost(xevent, host_);

  // Send key release events.
  xevent.xkey.type = x11::KeyEvent::Release;
  PostEventToWindowTreeHost(xevent, host_);
  if (alt)
    UnmaskAndSetKeycodeThenSend(&xevent, Mod1Mask, XK_Alt_L);
  if (shift)
    UnmaskAndSetKeycodeThenSend(&xevent, ShiftMask, XK_Shift_L);
  if (control)
    UnmaskAndSetKeycodeThenSend(&xevent, ControlMask, XK_Control_L);
  if (command)
    UnmaskAndSetKeycodeThenSend(&xevent, Mod4Mask, XK_Meta_L);
  DCHECK(!xevent.xkey.state);
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseMove(int screen_x, int screen_y) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
}

bool UIControlsX11::SendMouseMoveNotifyWhenDone(int screen_x,
                                                int screen_y,
                                                base::OnceClosure closure) {
  gfx::Point root_location(screen_x, screen_y);
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(host_->window());
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(host_->window(),
                                                   &root_location);
  }
  gfx::Point root_current_location =
      QueryLatestMousePositionRequestInHost(host_);
  host_->ConvertPixelsToDIP(&root_current_location);

  if (root_location != root_current_location && button_down_mask == 0) {
    // Move the cursor because EnterNotify/LeaveNotify are generated with the
    // current mouse position as a result of XGrabPointer()
    host_->window()->MoveCursorTo(root_location);
  } else {
    XEvent xevent;
    xevent.xmotion = {};
    XMotionEvent* xmotion = &xevent.xmotion;
    xmotion->type = MotionNotify;
    xmotion->x = root_location.x();
    xmotion->y = root_location.y();
    xmotion->state = button_down_mask;
    xmotion->same_screen = x11::True;
    // WindowTreeHost will take care of other necessary fields.
    PostEventToWindowTreeHost(xevent, host_);
  }
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseEvents(MouseButton type,
                                    int button_state,
                                    int accelerator_state) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state);
}

bool UIControlsX11::SendMouseEventsNotifyWhenDone(MouseButton type,
                                                  int button_state,
                                                  base::OnceClosure closure,
                                                  int accelerator_state) {
  XEvent xevent;
  xevent.xbutton = {};
  XButtonEvent* xbutton = &xevent.xbutton;
  gfx::Point mouse_loc = Env::GetInstance()->last_mouse_location();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(host_->window());
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(host_->window(), &mouse_loc);
  }
  xbutton->x = mouse_loc.x();
  xbutton->y = mouse_loc.y();
  xbutton->same_screen = x11::True;
  switch (type) {
    case LEFT:
      xbutton->button = 1;
      xbutton->state = Button1Mask;
      break;
    case MIDDLE:
      xbutton->button = 2;
      xbutton->state = Button2Mask;
      break;
    case RIGHT:
      xbutton->button = 3;
      xbutton->state = Button3Mask;
      break;
  }

  // Process accelerator key state.
  if (accelerator_state & ui_controls::kShift)
    xbutton->state |= ShiftMask;
  if (accelerator_state & ui_controls::kControl)
    xbutton->state |= ControlMask;
  if (accelerator_state & ui_controls::kAlt)
    xbutton->state |= Mod1Mask;
  if (accelerator_state & ui_controls::kCommand)
    xbutton->state |= Mod4Mask;

  // WindowEventDispatcher will take care of other necessary fields.
  if (button_state & DOWN) {
    xevent.xbutton.type = x11::ButtonEvent::Press;
    PostEventToWindowTreeHost(xevent, host_);
    button_down_mask |= xbutton->state;
  }
  if (button_state & UP) {
    xevent.xbutton.type = x11::ButtonEvent::Release;
    PostEventToWindowTreeHost(xevent, host_);
    button_down_mask = (button_down_mask | xbutton->state) ^ xbutton->state;
  }
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator);
}

void UIControlsX11::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  if (closure.is_null())
    return;
  ui::XEventWaiter::Create(
      static_cast<x11::Window>(host_->GetAcceleratedWidget()),
      std::move(closure));
}

void UIControlsX11::SetKeycodeAndSendThenMask(XEvent* xevent,
                                              KeySym keysym,
                                              unsigned int mask) {
  xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), keysym);
  PostEventToWindowTreeHost(*xevent, host_);
  xevent->xkey.state |= mask;
}

void UIControlsX11::UnmaskAndSetKeycodeThenSend(XEvent* xevent,
                                                unsigned int mask,
                                                KeySym keysym) {
  xevent->xkey.state ^= mask;
  xevent->xkey.keycode = XKeysymToKeycode(gfx::GetXDisplay(), keysym);
  PostEventToWindowTreeHost(*xevent, host_);
}

}  // namespace test
}  // namespace aura
