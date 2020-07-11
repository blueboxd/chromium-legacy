// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/input_event_conversions.h"

#include "base/notreached.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/var.h"

namespace {

chrome_pdf::InputEventType GetEventType(const PP_InputEvent_Type& input_type) {
  switch (input_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      return chrome_pdf::InputEventType::kMouseDown;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      return chrome_pdf::InputEventType::kMouseUp;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      return chrome_pdf::InputEventType::kMouseMove;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      return chrome_pdf::InputEventType::kMouseEnter;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return chrome_pdf::InputEventType::kMouseLeave;
    case PP_INPUTEVENT_TYPE_WHEEL:
      return chrome_pdf::InputEventType::kWheel;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      return chrome_pdf::InputEventType::kRawKeyDown;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      return chrome_pdf::InputEventType::kKeyDown;
    case PP_INPUTEVENT_TYPE_KEYUP:
      return chrome_pdf::InputEventType::kKeyUp;
    case PP_INPUTEVENT_TYPE_CHAR:
      return chrome_pdf::InputEventType::kChar;
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      return chrome_pdf::InputEventType::kContextMenu;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
      return chrome_pdf::InputEventType::kImeCompositionStart;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      return chrome_pdf::InputEventType::kImeCompositionUpdate;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
      return chrome_pdf::InputEventType::kImeCompositionEnd;
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      return chrome_pdf::InputEventType::kImeText;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      return chrome_pdf::InputEventType::kTouchStart;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      return chrome_pdf::InputEventType::kTouchMove;
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      return chrome_pdf::InputEventType::kTouchEnd;
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      return chrome_pdf::InputEventType::kTouchCancel;
    default:
      NOTREACHED();
      return chrome_pdf::InputEventType::kNone;
  };
}

}  // namespace

namespace chrome_pdf {

KeyboardInputEvent::KeyboardInputEvent(InputEventType event_type,
                                       double time_stamp,
                                       uint32_t modifiers,
                                       uint32_t keyboard_code,
                                       const std::string& key_char)
    : event_type_(event_type),
      time_stamp_(time_stamp),
      modifiers_(modifiers),
      keyboard_code_(keyboard_code),
      key_char_(key_char) {}

KeyboardInputEvent::KeyboardInputEvent(const KeyboardInputEvent& other) =
    default;

KeyboardInputEvent& KeyboardInputEvent::operator=(
    const KeyboardInputEvent& other) = default;

KeyboardInputEvent::~KeyboardInputEvent() = default;

KeyboardInputEvent GetKeyboardInputEvent(const pp::KeyboardInputEvent& event) {
  return KeyboardInputEvent(GetEventType(event.GetType()), event.GetTimeStamp(),
                            event.GetModifiers(), event.GetKeyCode(),
                            event.GetCharacterText().AsString());
}

}  // namespace chrome_pdf
