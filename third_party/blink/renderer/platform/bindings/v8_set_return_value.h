// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_value_cache.h"
#include "v8/include/v8.h"

namespace blink {

namespace bindings {

// V8SetReturnValue sets a return value in a V8 callback function.  The first
// two arguments are fixed as v8::{Function,Property}CallbackInfo and the
// return value.  V8SetReturnValue may take more arguments as optimization hints
// depending on the return value type.

struct V8ReturnValue {
  // Support compile-time overload resolution by making each value have its own
  // type.

  // Nullable or not
  enum NonNullable { kNonNullable };
  enum Nullable { kNullable };

  // Main world or not
  enum MainWorld { kMainWorld };
};

// V8 handle types
template <typename CallbackInfo, typename S>
void V8SetReturnValue(const CallbackInfo& info, const v8::Global<S> value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo, typename S>
void V8SetReturnValue(const CallbackInfo& info, const v8::Local<S> value) {
  info.GetReturnValue().Set(value);
}

// nullptr
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, nullptr_t) {
  info.GetReturnValue().SetNull();
}

// Primitive types
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, bool value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, int32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, uint32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info, double value) {
  info.GetReturnValue().Set(value);
}

// String types
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const AtomicString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetEmptyString();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const String& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::NonNullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetEmptyString();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const AtomicString& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetNull();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      const String& string,
                      v8::Isolate* isolate,
                      V8ReturnValue::Nullable) {
  if (string.IsNull())
    return info.GetReturnValue().SetNull();
  V8PerIsolateData::From(isolate)->GetStringCache()->SetReturnValueFromString(
      info.GetReturnValue(), string.Impl());
}

// ScriptWrappable
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable* value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  if (DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(), value))
    return;

  info.GetReturnValue().Set(value->Wrap(info.GetIsolate(), info.This()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable& value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  if (DOMDataStore::SetReturnValueForMainWorld(info.GetReturnValue(), &value))
    return;

  info.GetReturnValue().Set(value.Wrap(info.GetIsolate(), info.This()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable* value,
                      const ScriptWrappable* receiver) {
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), value,
                                       info.This(), receiver)) {
    return;
  }

  info.GetReturnValue().Set(value->Wrap(info.GetIsolate(), info.This()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable& value,
                      const ScriptWrappable* receiver) {
  if (DOMDataStore::SetReturnValueFast(info.GetReturnValue(), &value,
                                       info.This(), receiver)) {
    return;
  }

  info.GetReturnValue().Set(value.Wrap(info.GetIsolate(), info.This()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable* value,
                      v8::Local<v8::Context> creation_context) {
  if (UNLIKELY(!value))
    return info.GetReturnValue().SetNull();

  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), value))
    return;

  info.GetReturnValue().Set(
      value->Wrap(info.GetIsolate(), creation_context->Global()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      ScriptWrappable& value,
                      v8::Local<v8::Context> creation_context) {
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), &value))
    return;

  info.GetReturnValue().Set(
      value.Wrap(info.GetIsolate(), creation_context->Global()));
}

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_SET_RETURN_VALUE_H_
