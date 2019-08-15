// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_scope.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_work_task.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

CustomLayoutChild::CustomLayoutChild(const CSSLayoutDefinition& definition,
                                     LayoutBox* box)
    : box_(box),
      style_map_(MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          box->GetDocument(),
          box->StyleRef(),
          definition.ChildNativeInvalidationProperties(),
          definition.ChildCustomInvalidationProperties())) {}

ScriptPromise CustomLayoutChild::layoutNextFragment(
    ScriptState* script_state,
    const CustomLayoutConstraintsOptions* options,
    ExceptionState& exception_state) {
  // A layout child may be invalid if it has been removed from the tree (it is
  // possible for a web developer to hold onto a LayoutChild object after its
  // underlying LayoutObject has been destroyed).
  if (!box_ || !token_->IsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "The LayoutChild is not valid."));
  }

  // Serialize the provided data if needed.
  scoped_refptr<SerializedScriptValue> constraint_data;
  if (options->hasData()) {
    // We serialize "kForStorage" so that SharedArrayBuffers can't be shared
    // between LayoutWorkletGlobalScopes.
    constraint_data = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), options->data().V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kForStorage),
        exception_state);

    if (exception_state.HadException())
      return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  CustomLayoutScope::Current()->Queue()->emplace_back(
      this, token_, resolver, options, std::move(constraint_data));
  return resolver->Promise();
}

void CustomLayoutChild::Trace(blink::Visitor* visitor) {
  visitor->Trace(style_map_);
  visitor->Trace(token_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
