// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_big_int_64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_big_uint_64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_data_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_float64_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int16_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_int8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint16_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint32_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_clamped_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

namespace blink {

namespace bindings {

ScriptWrappable* NativeValueTraitsInterfaceNativeValue(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
    return ToScriptWrappable(value.As<v8::Object>());

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      wrapper_type_info->interface_name));
  return nullptr;
}

ScriptWrappable* NativeValueTraitsInterfaceArgumentValue(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
    return ToScriptWrappable(value.As<v8::Object>());

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, wrapper_type_info->interface_name));
  return nullptr;
}

ScriptWrappable* NativeValueTraitsInterfaceOrNullNativeValue(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
    return ToScriptWrappable(value.As<v8::Object>());

  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      wrapper_type_info->interface_name));
  return nullptr;
}

ScriptWrappable* NativeValueTraitsInterfaceOrNullArgumentValue(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
    return ToScriptWrappable(value.As<v8::Object>());

  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, wrapper_type_info->interface_name));
  return nullptr;
}

}  // namespace bindings

// Buffer source types
#define DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(T, V8T)             \
  T* NativeValueTraits<T>::NativeValue(v8::Isolate* isolate,              \
                                       v8::Local<v8::Value> value,        \
                                       ExceptionState& exception_state) { \
    T* native_value = V8T::ToImplWithTypeCheck(isolate, value);           \
    if (!native_value) {                                                  \
      exception_state.ThrowTypeError(                                     \
          ExceptionMessages::FailedToConvertJSValue(                      \
              V8T::GetWrapperTypeInfo()->interface_name));                \
    }                                                                     \
    return native_value;                                                  \
  }
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMArrayBuffer, V8ArrayBuffer)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMArrayBufferView,
                                              V8ArrayBufferView)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMInt8Array, V8Int8Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMInt16Array, V8Int16Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMInt32Array, V8Int32Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMUint8Array, V8Uint8Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMUint8ClampedArray,
                                              V8Uint8ClampedArray)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMUint16Array, V8Uint16Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMUint32Array, V8Uint32Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMBigInt64Array, V8BigInt64Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMBigUint64Array,
                                              V8BigUint64Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMFloat32Array, V8Float32Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMFloat64Array, V8Float64Array)
DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE(DOMDataView, V8DataView)
#undef DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE

// EventHandler
EventListener* NativeValueTraits<IDLEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kEventHandler);
}

EventListener* NativeValueTraits<IDLOnBeforeUnloadEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler);
}

EventListener* NativeValueTraits<IDLOnErrorEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kOnErrorEventHandler);
}

}  // namespace blink
