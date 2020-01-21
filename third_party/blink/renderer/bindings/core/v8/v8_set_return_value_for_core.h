// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

namespace blink {

namespace bindings {

// ScriptWrappable (DOMWindow specialization)
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow* value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  info.GetReturnValue().Set(ToV8(value, info.This(), info.GetIsolate()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow& value,
                      V8ReturnValue::MainWorld) {
  DCHECK(DOMWrapperWorld::Current(info.GetIsolate()).IsMainWorld());
  info.GetReturnValue().Set(ToV8(&value, info.This(), info.GetIsolate()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow* value,
                      const ScriptWrappable* receiver) {
  info.GetReturnValue().Set(ToV8(value, info.This(), info.GetIsolate()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow& value,
                      const ScriptWrappable* receiver) {
  info.GetReturnValue().Set(ToV8(&value, info.This(), info.GetIsolate()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow* value,
                      v8::Local<v8::Context> creation_context) {
  info.GetReturnValue().Set(ToV8(value, info.This(), info.GetIsolate()));
}

template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      DOMWindow& value,
                      v8::Local<v8::Context> creation_context) {
  info.GetReturnValue().Set(ToV8(&value, info.This(), info.GetIsolate()));
}

// EventListener
template <typename CallbackInfo>
void V8SetReturnValue(const CallbackInfo& info,
                      EventListener* value,
                      v8::Isolate* isolate,
                      EventTarget* event_target) {
  info.GetReturnValue().Set(
      JSEventHandler::AsV8Value(isolate, event_target, value));
}

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_SET_RETURN_VALUE_FOR_CORE_H_
