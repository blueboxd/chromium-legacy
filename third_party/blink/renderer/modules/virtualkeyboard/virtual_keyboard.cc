// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

VirtualKeyboard::VirtualKeyboard(LocalFrame* frame)
    : ExecutionContextClient(frame ? frame->DomWindow()->GetExecutionContext()
                                   : nullptr) {}

ExecutionContext* VirtualKeyboard::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& VirtualKeyboard::InterfaceName() const {
  return event_target_names::kVirtualKeyboard;
}

VirtualKeyboard::~VirtualKeyboard() = default;

bool VirtualKeyboard::overlaysContent() const {
  return overlays_content_;
}

void VirtualKeyboard::setOverlaysContent(bool overlays_content) {
  // TODO(snianu): Fill this function.
}

void VirtualKeyboard::VirtualKeyboardOverlayChanged(
    const gfx::Rect& keyboard_rect) {
  // TODO(snianu): Fill this function.
}

void VirtualKeyboard::show() {
  // TODO(snianu): Fill this function.
}

void VirtualKeyboard::hide() {
  // TODO(snianu): Fill this function.
}

void VirtualKeyboard::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
