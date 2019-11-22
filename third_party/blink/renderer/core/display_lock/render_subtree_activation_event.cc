// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/render_subtree_activation_event.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

const AtomicString& RenderSubtreeActivationEvent::InterfaceName() const {
  return event_interface_names::kRenderSubtreeActivationEvent;
}

RenderSubtreeActivationEvent::RenderSubtreeActivationEvent(
    Element& activated_element)
    : Event(event_type_names::kRendersubtreeactivation,
            Bubbles::kYes,
            Cancelable::kYes,
            ComposedMode::kScoped),
      activated_element_(activated_element) {}

void RenderSubtreeActivationEvent::Trace(Visitor* visitor) {
  visitor->Trace(activated_element_);
  Event::Trace(visitor);
}

}  // namespace blink
