// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_

#include <wayland-client.h>

#include "base/time/time.h"
#include "ui/base/buildflags.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class DomKey;
class KeyboardLayoutEngine;
class WaylandConnection;
class WaylandWindow;
#if BUILDFLAG(USE_XKBCOMMON)
class XkbKeyboardLayoutEngine;
#endif

class WaylandKeyboard : public EventAutoRepeatHandler::Delegate {
 public:
  class Delegate;

  WaylandKeyboard(wl_keyboard* keyboard,
                  WaylandConnection* connection,
                  KeyboardLayoutEngine* keyboard_layout_engine,
                  Delegate* delegate);
  virtual ~WaylandKeyboard();

  int device_id() const { return obj_.id(); }
  bool Decode(DomCode dom_code,
              int modifiers,
              DomKey* out_dom_key,
              KeyboardCode* out_key_code);

 private:
  // wl_keyboard_listener
  static void Keymap(void* data,
                     wl_keyboard* obj,
                     uint32_t format,
                     int32_t fd,
                     uint32_t size);
  static void Enter(void* data,
                    wl_keyboard* obj,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_array* keys);
  static void Leave(void* data,
                    wl_keyboard* obj,
                    uint32_t serial,
                    wl_surface* surface);
  static void Key(void* data,
                  wl_keyboard* obj,
                  uint32_t serial,
                  uint32_t time,
                  uint32_t key,
                  uint32_t state);
  static void Modifiers(void* data,
                        wl_keyboard* obj,
                        uint32_t serial,
                        uint32_t mods_depressed,
                        uint32_t mods_latched,
                        uint32_t mods_locked,
                        uint32_t group);
  static void RepeatInfo(void* data,
                         wl_keyboard* obj,
                         int32_t rate,
                         int32_t delay);

  static void SyncCallback(void* data, struct wl_callback* cb, uint32_t time);

  // EventAutoRepeatHandler::Delegate
  void FlushInput(base::OnceClosure closure) override;
  void DispatchKey(unsigned int key,
                   bool down,
                   bool repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags) override;

  wl::Object<wl_keyboard> obj_;
  WaylandConnection* const connection_;
  Delegate* const delegate_;

  // Key repeat handler.
  static const wl_callback_listener callback_listener_;
  EventAutoRepeatHandler auto_repeat_handler_;
  base::OnceClosure auto_repeat_closure_;
  wl::Object<wl_callback> sync_callback_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbKeyboardLayoutEngine* layout_engine_;
#else
  KeyboardLayoutEngine* layout_engine_;
#endif
};

class WaylandKeyboard::Delegate {
 public:
  virtual void OnKeyboardCreated(WaylandKeyboard* keyboard) = 0;
  virtual void OnKeyboardDestroyed(WaylandKeyboard* keyboard) = 0;
  virtual void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) = 0;
  virtual void OnKeyboardModifiersChanged(int modifiers) = 0;
  virtual void OnKeyboardKeyEvent(EventType type,
                                  DomCode dom_code,
                                  DomKey dom_key,
                                  KeyboardCode key_code,
                                  bool repeat,
                                  base::TimeTicks timestamp) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_
