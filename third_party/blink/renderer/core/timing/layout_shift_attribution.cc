// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift_attribution.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

LayoutShiftAttribution::LayoutShiftAttribution() = default;

LayoutShiftAttribution::~LayoutShiftAttribution() = default;

Node* LayoutShiftAttribution::node() const {
  // TODO(crbug.com/1053510): Implement.
  return nullptr;
}

DOMRectReadOnly* LayoutShiftAttribution::previousRect() const {
  // TODO(crbug.com/1053510): Implement.
  return nullptr;
}

DOMRectReadOnly* LayoutShiftAttribution::currentRect() const {
  // TODO(crbug.com/1053510): Implement.
  return nullptr;
}

ScriptValue LayoutShiftAttribution::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  // TODO(crbug.com/1053510): Implement.
  return builder.GetScriptValue();
}

void LayoutShiftAttribution::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
