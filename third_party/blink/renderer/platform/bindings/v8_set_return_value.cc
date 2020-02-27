// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

namespace bindings {

v8::Local<v8::Value> GetInterfaceObjectExposedOnGlobal(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      isolate, "Blink_GetInterfaceObjectExposedOnGlobal");
  V8PerContextData* per_context_data =
      V8PerContextData::From(creation_context->CreationContext());
  if (!per_context_data)
    return v8::Local<v8::Value>();
  return per_context_data->ConstructorForType(wrapper_type_info);
}

}  // namespace bindings

}  // namespace blink
