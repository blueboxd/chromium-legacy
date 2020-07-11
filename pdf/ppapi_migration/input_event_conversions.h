// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_
#define PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_

#include <stdint.h>
#include <string>

namespace pp {
class KeyboardInputEvent;
}  // namespace pp

namespace chrome_pdf {

enum InputEventModifier : uint32_t {
  // None represents no modifier key specified.
  kInputEventModifierNone = 0,
  kInputEventModifierShiftKey = 1 << 0,
  kInputEventModifierControlKey = 1 << 1,
  kInputEventModifierAltKey = 1 << 2,
  kInputEventModifierMetaKey = 1 << 3,
  kInputEventModifierIsKeyPad = 1 << 4,
  kInputEventModifierIsAutoRepeat = 1 << 5,
  kInputEventModifierLeftButtonDown = 1 << 6,
  kInputEventModifierMiddleButtonDown = 1 << 7,
  kInputEventModifierRightButtonDown = 1 << 8,
  kInputEventModifierCapsLockKey = 1 << 9,
  kInputEventModifierNumLockKey = 1 << 10,
  kInputEventModifierIsLeft = 1 << 11,
  kInputEventModifierIsRight = 1 << 12,
  kInputEventModifierIsPen = 1 << 13,
  kInputEventModifierIsEraser = 1 << 14
};

enum class InputEventType {
  kNone,

  // Notification that a mouse button was pressed.
  kMouseDown,

  // Notification that a mouse button was released.
  kMouseUp,

  // Notification that a mouse button was moved when it is over the instance
  // or dragged out of it.
  kMouseMove,

  // Notification that the mouse entered the pdf view's bounds.
  kMouseEnter,

  // Notification that a mouse left the pdf view's bounds.
  kMouseLeave,

  // Notification that the scroll wheel was used.
  kWheel,

  // Notification that a key transitioned from "up" to "down".
  kRawKeyDown,

  // Notification that a key was pressed. This does not necessarily correspond
  // to a character depending on the key and language. Use the
  // kChar for character input.
  kKeyDown,

  // Notification that a key was released.
  kKeyUp,

  // Notification that a character was typed. Use this for text input. Key
  // down events may generate 0, 1, or more than one character event depending
  // on the key, locale, and operating system.
  kChar,

  kContextMenu,

  // Notification that an input method composition process has just started.
  kImeCompositionStart,

  // Notification that the input method composition string is updated.
  kImeCompositionUpdate,

  // Notification that an input method composition process has completed.
  kImeCompositionEnd,

  // Notification that an input method committed a string.
  kImeText,

  // Notification that a finger was placed on a touch-enabled device.
  kTouchStart,

  // Notification that a finger was moved on a touch-enabled device.
  kTouchMove,

  // Notification that a finger was released on a touch-enabled device.
  kTouchEnd,

  // Notification that a touch event was canceled.
  kTouchCancel
};

class KeyboardInputEvent {
 public:
  KeyboardInputEvent(InputEventType event_type,
                     double time_stamp,
                     uint32_t modifiers,
                     uint32_t keyboard_code,
                     const std::string& key_char);
  KeyboardInputEvent(const KeyboardInputEvent& other);
  KeyboardInputEvent& operator=(const KeyboardInputEvent& other);
  ~KeyboardInputEvent();

  const InputEventType& GetEventType() const { return event_type_; }

  double GetTimeStamp() const { return time_stamp_; }

  uint32_t GetModifiers() const { return modifiers_; }

  uint32_t GetKeyCode() const { return keyboard_code_; }

  const std::string& GetKeyChar() const { return key_char_; }

 private:
  InputEventType event_type_ = InputEventType::kNone;
  // The units are in seconds, but are not measured relative to any particular
  // epoch, so the most you can do is compare two values.
  double time_stamp_ = 0;
  uint32_t modifiers_ = kInputEventModifierNone;
  uint32_t keyboard_code_;
  std::string key_char_;
};

KeyboardInputEvent GetKeyboardInputEvent(const pp::KeyboardInputEvent& event);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_
