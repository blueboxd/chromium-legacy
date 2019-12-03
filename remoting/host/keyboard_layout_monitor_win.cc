// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include <windows.h>
#include <ime.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/timer/timer.h"
#include "remoting/proto/control.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

constexpr base::TimeDelta POLL_INTERVAL =
    base::TimeDelta::FromMilliseconds(1000);

class KeyboardLayoutMonitorWin : public KeyboardLayoutMonitor {
 public:
  KeyboardLayoutMonitorWin(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);
  ~KeyboardLayoutMonitorWin() override;
  void Start() override;

 private:
  // Check the current layout, and invoke the callback if it has changed.
  void QueryLayout();

  void ClearDeadKeys(HKL layout);

  bool IsNumpadKey(ui::DomCode code);

  UINT TranslateVirtualKey(bool numlock_state,
                           bool shift_state,
                           UINT virtual_key,
                           ui::DomCode code);

  protocol::LayoutKeyFunction VirtualKeyToLayoutKeyFunction(UINT virtual_key,
                                                            LANGID lang);

  HKL previous_layout_ = nullptr;
  base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback_;
  base::RepeatingTimer timer_;
};

KeyboardLayoutMonitorWin::KeyboardLayoutMonitorWin(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : callback_(std::move(callback)) {}

KeyboardLayoutMonitorWin::~KeyboardLayoutMonitorWin() = default;

void KeyboardLayoutMonitorWin::Start() {
  timer_.Start(FROM_HERE, POLL_INTERVAL, this,
               &KeyboardLayoutMonitorWin::QueryLayout);
}

void KeyboardLayoutMonitorWin::QueryLayout() {
  // Get the keyboard layout for the active window.
  DWORD thread_id = 0;
  HWND foreground_window = GetForegroundWindow();
  if (foreground_window) {
    thread_id = GetWindowThreadProcessId(foreground_window, nullptr);
  } else if (previous_layout_ != 0) {
    // There's no currently active window, so keep using the previous layout.
    return;
  }
  // If there is no previous layout and there's no active window
  // (thread_id == 0), this will return the layout associated with this
  // thread, which is better than nothing.
  HKL layout = GetKeyboardLayout(thread_id);
  if (layout == previous_layout_) {
    return;
  }
  previous_layout_ = layout;

  protocol::KeyboardLayout layout_message;
  // TODO(rkjnsn): Windows doesn't provide an API to read the keyboard layout
  // directly. We can use the various key translation functions to mostly get
  // the information we need, but it requires some hacks, could miss some edge
  // cases, and modifies the system keyboard state. Windows keyboard layouts
  // consist of DLLs providing data tables in a reasonable straight-forward
  // format, so it may make sense to look at reading them directly.

  // It would be nice if we could use MapVirtualKeyEx to translate virtual
  // keys to characters, as it neither is affected by nor modifies the system
  // keyboard state. Unfortunately, it provides no way to specify shift
  // states, so it can only be used to retrieve the unshifted character for a
  // given key. Instead, we use ToUnicodeEx, which is affected by (and
  // affects) the system keyboard state, so we start by clearing any queued-up
  // dead keys.
  ClearDeadKeys(layout);

  // Keyboard layouts have a KLLF_ALTGR flag that indicated whether the right
  // alt key is AltGr (and thus generates Ctrl+Alt). Unfortunately, there
  // doesn't seem to be any way to determine whether the current layout sets
  // this flag using the Windows API. As a hack/workaround, we assume that
  // right Alt == AltGr if and only if there are keys that generate characters
  // when both Ctrl and Alt modifiers are set. This is by no means guaranteed,
  // but is expected to be true for common layouts.
  bool has_altgr = false;

  // Keys/shift levels that function as right Alt. These will be updated to
  // AltGr if the keyboard appears to use it. (Most keyboards will have at
  // most one of these.)
  std::vector<std::pair<std::uint32_t, int>> right_alts;

  for (ui::DomCode key : KeyboardLayoutMonitor::kSupportedKeys) {
    std::uint32_t usb_code = ui::KeycodeConverter::DomCodeToUsbKeycode(key);
    int scancode = ui::KeycodeConverter::DomCodeToNativeKeycode(key);
    UINT virtual_key = MapVirtualKeyEx(scancode, MAPVK_VSC_TO_VK_EX, layout);
    if (virtual_key == 0) {
      // This key is not mapped in the current layout.
      continue;
    }

    google::protobuf::Map<google::protobuf::uint32,
                          protocol::KeyboardLayout_KeyAction>& key_actions =
        *(*layout_message.mutable_keys())[usb_code].mutable_actions();

    for (int shift_level = 0; shift_level < 4; ++shift_level) {
      // Mimic Windows's handling of number pad key.
      UINT translated_key = TranslateVirtualKey(
          /* numlock_state */ true, shift_level & 1, virtual_key, key);

      // First check if the key generates a character.
      BYTE key_state[256] = {0};
      // Modifiers set the high-order bit when pressed.
      key_state[VK_SHIFT] = (shift_level & 1) << 7;
      key_state[VK_CONTROL] = key_state[VK_MENU] = (shift_level & 2) << 6;
      // Locks set the low-order bit when toggled on.
      // For now, generate a layout with numlock always on and caps lock
      // always off.
      // TODO(rkjnsn): Update this when we decide how we want to handle locks
      // for the on-screen keyboard.
      key_state[VK_NUMLOCK] = 1;
      key_state[VK_CAPITAL] = 0;
      WCHAR char_buffer[16];
      int size = ToUnicodeEx(translated_key, scancode, key_state, char_buffer,
                             base::size(char_buffer), 0, layout);
      if (size < 0) {
        // We don't handle dead keys specially for the layout, but we do
        // need to clear them from the system keyboard state.
        ClearDeadKeys(layout);
        size = -size;
      }
      if (size > 0) {
        // The key generated at least one character.
        key_actions[shift_level].set_character(
            base::UTF16ToUTF8(base::StringPiece16(char_buffer, size)));
        if (shift_level > 2) {
          has_altgr = true;
        }
        continue;
      }

      // If the key didn't generate a character, translate it based on the
      // virtual key value.
      key_actions[shift_level].set_function(VirtualKeyToLayoutKeyFunction(
          translated_key, reinterpret_cast<std::uintptr_t>(layout) & 0xFFFF));
      if (translated_key == VK_RMENU) {
        right_alts.emplace_back(usb_code, shift_level);
      }
    }
  }

  // If any ctrl+alt+key sequence generated a character, assume right Alt is
  // AltGr.
  if (has_altgr) {
    for (std::pair<std::uint32_t, int> right_alt : right_alts) {
      (*(*layout_message.mutable_keys())[right_alt.first]
            .mutable_actions())[right_alt.second]
          .set_function(protocol::LayoutKeyFunction::ALT_GR);
    }
  } else {
    // Remove higher shift levels since there's no way to generate them.
    for (auto& key : *layout_message.mutable_keys()) {
      key.second.mutable_actions()->erase(2);
      key.second.mutable_actions()->erase(3);
    }
  }

  callback_.Run(std::move(layout_message));
}

void KeyboardLayoutMonitorWin::ClearDeadKeys(HKL layout) {
  // ToUnicodeEx both is affected by and modifies the current keyboard state,
  // which includes the list of currently stored dead keys. Pressing space
  // translates previously pressed dead keys to characters, clearing the dead-
  // key buffer.
  BYTE key_state[256] = {0};
  WCHAR char_buffer[16];
  ToUnicodeEx(VK_SPACE,
              ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::SPACE),
              key_state, char_buffer, base::size(char_buffer), 0, layout);
}

bool KeyboardLayoutMonitorWin::IsNumpadKey(ui::DomCode code) {
  // Windows keyboard layouts map number pad keys to virtual keys based on
  // their function when num lock is off. E.g., 4 on the number pad generates
  // VK_LEFT, the same as the left arrow key. To distinguish them, the layout
  // sets the KBDNUMPAD flag on the numlock versions, allowing Windows to
  // translate VK_LEFT to VK_NUMPAD4 when numlock is enabled only for the keys
  // on the number pad. Unfortunately, the state of the KBDNUMPAD flag for a
  // given scan code does not appear to be accessible via the Windows API, so
  // for now just assume that all layouts use the same scan codes for the
  // number pad.
  // TODO(rkjnsn): Figure out if there's a better way to determine this.

  switch (code) {
    case ui::DomCode::NUMPAD0:
    case ui::DomCode::NUMPAD1:
    case ui::DomCode::NUMPAD2:
    case ui::DomCode::NUMPAD3:
    case ui::DomCode::NUMPAD4:
    case ui::DomCode::NUMPAD5:
    case ui::DomCode::NUMPAD6:
    case ui::DomCode::NUMPAD7:
    case ui::DomCode::NUMPAD8:
    case ui::DomCode::NUMPAD9:
    case ui::DomCode::NUMPAD_DECIMAL:
      return true;
    default:
      return false;
  }
}

UINT KeyboardLayoutMonitorWin::TranslateVirtualKey(bool numlock_state,
                                                   bool shift_state,
                                                   UINT virtual_key,
                                                   ui::DomCode code) {
  // Windows only translates numpad keys when num lock is on and shift is not
  // pressed. (Pressing shift when num lock is on will get you navigation, but
  // pressing shift when num lock is off will not get you numbers.)
  if (!numlock_state || shift_state || !IsNumpadKey(code)) {
    return virtual_key;
  }
  switch (virtual_key) {
    case VK_INSERT:
      return VK_NUMPAD0;
    case VK_END:
      return VK_NUMPAD1;
    case VK_DOWN:
      return VK_NUMPAD2;
    case VK_NEXT:
      return VK_NUMPAD3;
    case VK_LEFT:
      return VK_NUMPAD4;
    case VK_CLEAR:
      return VK_NUMPAD5;
    case VK_RIGHT:
      return VK_NUMPAD6;
    case VK_HOME:
      return VK_NUMPAD7;
    case VK_UP:
      return VK_NUMPAD8;
    case VK_PRIOR:
      return VK_NUMPAD9;
    default:
      return virtual_key;
  }
}

protocol::LayoutKeyFunction
KeyboardLayoutMonitorWin::VirtualKeyToLayoutKeyFunction(UINT virtual_key,
                                                        LANGID lang) {
  switch (virtual_key) {
    case VK_LCONTROL:
    case VK_RCONTROL:
      return protocol::LayoutKeyFunction::CONTROL;
    case VK_LMENU:
    case VK_RMENU:
      return protocol::LayoutKeyFunction::ALT;
    case VK_LSHIFT:
    case VK_RSHIFT:
      return protocol::LayoutKeyFunction::SHIFT;
    case VK_LWIN:
    case VK_RWIN:
      return protocol::LayoutKeyFunction::META;
    case VK_NUMLOCK:
      return protocol::LayoutKeyFunction::NUM_LOCK;
    case VK_CAPITAL:
      return protocol::LayoutKeyFunction::CAPS_LOCK;
    case VK_SCROLL:
      return protocol::LayoutKeyFunction::SCROLL_LOCK;
    case VK_BACK:
      return protocol::LayoutKeyFunction::BACKSPACE;
    case VK_RETURN:
      return protocol::LayoutKeyFunction::ENTER;
    case VK_TAB:
      return protocol::LayoutKeyFunction::TAB;
    case VK_INSERT:
      return protocol::LayoutKeyFunction::INSERT;
    case VK_DELETE:
      return protocol::LayoutKeyFunction::DELETE_;
    case VK_HOME:
      return protocol::LayoutKeyFunction::HOME;
    case VK_END:
      return protocol::LayoutKeyFunction::END;
    case VK_PRIOR:
      return protocol::LayoutKeyFunction::PAGE_UP;
    case VK_NEXT:
      return protocol::LayoutKeyFunction::PAGE_DOWN;
    case VK_CLEAR:
      return protocol::LayoutKeyFunction::CLEAR;
    case VK_UP:
      return protocol::LayoutKeyFunction::ARROW_UP;
    case VK_DOWN:
      return protocol::LayoutKeyFunction::ARROW_DOWN;
    case VK_LEFT:
      return protocol::LayoutKeyFunction::ARROW_LEFT;
    case VK_RIGHT:
      return protocol::LayoutKeyFunction::ARROW_RIGHT;
    case VK_F1:
      return protocol::LayoutKeyFunction::F1;
    case VK_F2:
      return protocol::LayoutKeyFunction::F2;
    case VK_F3:
      return protocol::LayoutKeyFunction::F3;
    case VK_F4:
      return protocol::LayoutKeyFunction::F4;
    case VK_F5:
      return protocol::LayoutKeyFunction::F5;
    case VK_F6:
      return protocol::LayoutKeyFunction::F6;
    case VK_F7:
      return protocol::LayoutKeyFunction::F7;
    case VK_F8:
      return protocol::LayoutKeyFunction::F8;
    case VK_F9:
      return protocol::LayoutKeyFunction::F9;
    case VK_F10:
      return protocol::LayoutKeyFunction::F10;
    case VK_F11:
      return protocol::LayoutKeyFunction::F11;
    case VK_F12:
      return protocol::LayoutKeyFunction::F12;
    case VK_F13:
      return protocol::LayoutKeyFunction::F13;
    case VK_F14:
      return protocol::LayoutKeyFunction::F14;
    case VK_F15:
      return protocol::LayoutKeyFunction::F15;
    case VK_F16:
      return protocol::LayoutKeyFunction::F16;
    case VK_F17:
      return protocol::LayoutKeyFunction::F17;
    case VK_F18:
      return protocol::LayoutKeyFunction::F18;
    case VK_F19:
      return protocol::LayoutKeyFunction::F19;
    case VK_F20:
      return protocol::LayoutKeyFunction::F20;
    case VK_F21:
      return protocol::LayoutKeyFunction::F21;
    case VK_F22:
      return protocol::LayoutKeyFunction::F22;
    case VK_F23:
      return protocol::LayoutKeyFunction::F23;
    case VK_F24:
      return protocol::LayoutKeyFunction::F24;
    case VK_ESCAPE:
      return protocol::LayoutKeyFunction::ESCAPE;
    case VK_APPS:
      return protocol::LayoutKeyFunction::CONTEXT_MENU;
    case VK_PAUSE:
      return protocol::LayoutKeyFunction::PAUSE;
    case VK_PRINT:
      return protocol::LayoutKeyFunction::PRINT_SCREEN;
  }

  // Handle language-specific keys.
  if (PRIMARYLANGID(lang) == 0x11) {  // Japanese
    switch (virtual_key) {
      case VK_DBE_SBCSCHAR:
        return protocol::LayoutKeyFunction::HANKAKU_ZENKAKU_KANJI;
      case VK_CONVERT:
        return protocol::LayoutKeyFunction::HENKAN;
      case VK_NONCONVERT:
        return protocol::LayoutKeyFunction::MUHENKAN;
      case VK_DBE_KATAKANA:
      case VK_DBE_HIRAGANA:
        // TODO(rkjnsn): Make sure it makes sense to use the same key cap for
        // both of these.
        return protocol::LayoutKeyFunction::KATAKANA_HIRAGANA_ROMAJI;
      case VK_DBE_ALPHANUMERIC:
        return protocol::LayoutKeyFunction::EISU;
    }
  } else if (PRIMARYLANGID(lang) == 0x12) {  // Korean
    switch (virtual_key) {
      case VK_HANJA:
        return protocol::LayoutKeyFunction::HANJA;
      case VK_HANGUL:
        return protocol::LayoutKeyFunction::HAN_YEONG;
    }
  }

  return protocol::LayoutKeyFunction::UNKNOWN;
}

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback) {
  return std::make_unique<KeyboardLayoutMonitorWin>(std::move(callback));
}

}  // namespace remoting
