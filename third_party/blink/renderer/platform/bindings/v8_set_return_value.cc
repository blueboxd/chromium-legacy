// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

namespace bindings {

v8::Local<v8::Object> CreatePropertyDescriptorObject(
    v8::Isolate* isolate,
    const v8::PropertyDescriptor& desc) {
  // https://tc39.es/ecma262/#sec-frompropertydescriptor
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::Local<v8::Object> object = v8::Object::New(isolate);

  auto add_property = [&](const char* name, v8::Local<v8::Value> value) {
    return object->CreateDataProperty(current_context, V8String(isolate, name),
                                      value);
  };
  auto add_property_bool = [&](const char* name, bool value) {
    return add_property(name, value ? v8::True(isolate) : v8::False(isolate));
  };

  bool result;
  if (desc.has_value()) {
    if (!(add_property("value", desc.value()).To(&result) &&
          add_property_bool("writable", desc.writable()).To(&result)))
      return v8::Local<v8::Object>();
  } else {
    if (!(add_property("get", desc.get()).To(&result) &&
          add_property("set", desc.set()).To(&result)))
      return v8::Local<v8::Object>();
  }
  if (!(add_property_bool("enumerable", desc.enumerable()).To(&result) &&
        add_property_bool("configurable", desc.configurable()).To(&result)))
    return v8::Local<v8::Object>();

  return object;
}

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
