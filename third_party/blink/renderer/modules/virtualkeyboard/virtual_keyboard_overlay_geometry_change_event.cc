// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard_overlay_geometry_change_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_virtual_keyboard_overlay_geometry_change_event_init.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"

namespace blink {

VirtualKeyboardOverlayGeometryChangeEvent*
VirtualKeyboardOverlayGeometryChangeEvent::Create(
    const AtomicString& type,
    const VirtualKeyboardOverlayGeometryChangeEventInit* initializer) {
  return MakeGarbageCollected<VirtualKeyboardOverlayGeometryChangeEvent>(
      type, initializer);
}

VirtualKeyboardOverlayGeometryChangeEvent::
    VirtualKeyboardOverlayGeometryChangeEvent(
        const AtomicString& type,
        const VirtualKeyboardOverlayGeometryChangeEventInit* initializer)
    : Event(type, initializer) {}

VirtualKeyboardOverlayGeometryChangeEvent::
    VirtualKeyboardOverlayGeometryChangeEvent(const AtomicString& type,
                                              DOMRect* rect)
    : Event(type, Bubbles::kNo, Cancelable::kNo), bounding_rect_(rect) {}

void VirtualKeyboardOverlayGeometryChangeEvent::Trace(Visitor* visitor) {
  visitor->Trace(bounding_rect_);
  Event::Trace(visitor);
}

}  // namespace blink
