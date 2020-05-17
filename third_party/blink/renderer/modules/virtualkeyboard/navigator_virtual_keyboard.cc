// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/navigator_virtual_keyboard.h"

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

namespace blink {

// static
const char NavigatorVirtualKeyboard::kSupplementName[] =
    "NavigatorVirtualKeyboard";

NavigatorVirtualKeyboard::NavigatorVirtualKeyboard(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      virtual_keyboard_(MakeGarbageCollected<VirtualKeyboard>(
          GetSupplementable()->GetFrame())) {}

// static
VirtualKeyboard* NavigatorVirtualKeyboard::virtualKeyboard(
    Navigator& navigator) {
  NavigatorVirtualKeyboard* supplement =
      Supplement<Navigator>::From<NavigatorVirtualKeyboard>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorVirtualKeyboard>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->virtual_keyboard_;
}

void NavigatorVirtualKeyboard::Trace(Visitor* visitor) const {
  visitor->Trace(virtual_keyboard_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
