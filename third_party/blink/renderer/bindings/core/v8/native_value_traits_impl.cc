// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/custom/v8_custom_xpath_ns_resolver.h"
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
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

namespace blink {

namespace bindings {

static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kDefault) == kNormalConversion,
              "IDLIntegerConvMode::kDefault == kNormalConversion");
static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kClamp) == kClamp,
              "IDLIntegerConvMode::kClamp == kClamp");
static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kEnforceRange) == kEnforceRange,
              "IDLIntegerConvMode::kEnforceRange == kEnforceRange");

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

namespace {

enum class IDLBufferSourceTypeConvMode {
  kDefault,
  kNullable,
};

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
T* NativeValueTraitsBufferSourcePtrNativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  T* native_value = V8T::ToImplWithTypeCheck(isolate, value);
  if (native_value)
    return native_value;

  if (mode == IDLBufferSourceTypeConvMode::kNullable) {
    if (value->IsNullOrUndefined())
      return nullptr;
  }

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      V8T::GetWrapperTypeInfo()->interface_name));
  return nullptr;
}

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
T* NativeValueTraitsBufferSourcePtrArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  T* native_value = V8T::ToImplWithTypeCheck(isolate, value);
  if (native_value)
    return native_value;

  if (mode == IDLBufferSourceTypeConvMode::kNullable) {
    if (value->IsNullOrUndefined())
      return nullptr;
  }

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, V8T::GetWrapperTypeInfo()->interface_name));
  return nullptr;
}

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
NotShared<T> NativeValueTraitsBufferSourceNotSharedNativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  T* native_value = NativeValueTraitsBufferSourcePtrNativeValue<T, V8T, mode>(
      isolate, value, exception_state);
  if (!native_value)
    return NotShared<T>();
  if (native_value->IsShared()) {
    exception_state.ThrowTypeError(
        "The provided ArrayBufferView value must not be shared.");
    return NotShared<T>();
  }
  return NotShared<T>(native_value);
}

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
NotShared<T> NativeValueTraitsBufferSourceNotSharedArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  T* native_value = NativeValueTraitsBufferSourcePtrArgumentValue<T, V8T, mode>(
      isolate, argument_index, value, exception_state);
  if (!native_value)
    return NotShared<T>();
  if (native_value->IsShared()) {
    exception_state.ThrowTypeError(
        "The provided ArrayBufferView value must not be shared.");
    return NotShared<T>();
  }
  return NotShared<T>(native_value);
}

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
MaybeShared<T> NativeValueTraitsBufferSourceMaybeSharedNativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return MaybeShared<T>(
      NativeValueTraitsBufferSourcePtrNativeValue<T, V8T, mode>(
          isolate, value, exception_state));
}

template <
    typename T,
    typename V8T,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
MaybeShared<T> NativeValueTraitsBufferSourceMaybeSharedArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return MaybeShared<T>(
      NativeValueTraitsBufferSourcePtrArgumentValue<T, V8T, mode>(
          isolate, argument_index, value, exception_state));
}

template <
    typename T,
    typename V8T,
    typename FLEXIBLE,
    IDLBufferSourceTypeConvMode mode = IDLBufferSourceTypeConvMode::kDefault>
FLEXIBLE NativeValueTraitsBufferSourceFlexibleArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (V8T::HasInstance(isolate, value))
    return FLEXIBLE(value.As<v8::ArrayBufferView>());

  if (mode == IDLBufferSourceTypeConvMode::kNullable) {
    if (value->IsNullOrUndefined())
      return FLEXIBLE();
  }

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, V8T::GetWrapperTypeInfo()->interface_name));
  return FLEXIBLE();
}

}  // namespace

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueTraitsBufferSourcePtrNativeValue<DOMArrayBuffer,
                                                     V8ArrayBuffer>(
      isolate, value, exception_state);
}

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueTraitsBufferSourcePtrArgumentValue<DOMArrayBuffer,
                                                       V8ArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueTraitsBufferSourcePtrNativeValue<
      DOMArrayBuffer, V8ArrayBuffer, IDLBufferSourceTypeConvMode::kNullable>(
      isolate, value, exception_state);
}

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueTraitsBufferSourcePtrArgumentValue<
      DOMArrayBuffer, V8ArrayBuffer, IDLBufferSourceTypeConvMode::kNullable>(
      isolate, argument_index, value, exception_state);
}

#define DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_NOT_SHARED(T, V8T)    \
  template <>                                                               \
  CORE_EXPORT NotShared<T> NativeValueTraits<NotShared<T>>::NativeValue(    \
      v8::Isolate* isolate, v8::Local<v8::Value> value,                     \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceNotSharedNativeValue<T, V8T>(       \
        isolate, value, exception_state);                                   \
  }                                                                         \
  template <>                                                               \
  CORE_EXPORT NotShared<T> NativeValueTraits<NotShared<T>>::ArgumentValue(  \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value, \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceNotSharedArgumentValue<T, V8T>(     \
        isolate, argument_index, value, exception_state);                   \
  }                                                                         \
  template <>                                                               \
  CORE_EXPORT NotShared<T>                                                  \
  NativeValueTraits<IDLNullable<NotShared<T>>>::NativeValue(                \
      v8::Isolate* isolate, v8::Local<v8::Value> value,                     \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceNotSharedNativeValue<               \
        T, V8T, IDLBufferSourceTypeConvMode::kNullable>(isolate, value,     \
                                                        exception_state);   \
  }                                                                         \
  template <>                                                               \
  CORE_EXPORT NotShared<T>                                                  \
  NativeValueTraits<IDLNullable<NotShared<T>>>::ArgumentValue(              \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value, \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceNotSharedArgumentValue<             \
        T, V8T, IDLBufferSourceTypeConvMode::kNullable>(                    \
        isolate, argument_index, value, exception_state);                   \
  }
#define DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_MAYBE_SHARED(T, V8T)     \
  template <>                                                                  \
  CORE_EXPORT MaybeShared<T> NativeValueTraits<MaybeShared<T>>::NativeValue(   \
      v8::Isolate* isolate, v8::Local<v8::Value> value,                        \
      ExceptionState& exception_state) {                                       \
    return NativeValueTraitsBufferSourceMaybeSharedNativeValue<T, V8T>(        \
        isolate, value, exception_state);                                      \
  }                                                                            \
  template <>                                                                  \
  CORE_EXPORT MaybeShared<T> NativeValueTraits<MaybeShared<T>>::ArgumentValue( \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value,    \
      ExceptionState& exception_state) {                                       \
    return NativeValueTraitsBufferSourceMaybeSharedArgumentValue<T, V8T>(      \
        isolate, argument_index, value, exception_state);                      \
  }                                                                            \
  template <>                                                                  \
  CORE_EXPORT MaybeShared<T>                                                   \
  NativeValueTraits<IDLNullable<MaybeShared<T>>>::NativeValue(                 \
      v8::Isolate* isolate, v8::Local<v8::Value> value,                        \
      ExceptionState& exception_state) {                                       \
    return NativeValueTraitsBufferSourceMaybeSharedNativeValue<                \
        T, V8T, IDLBufferSourceTypeConvMode::kNullable>(isolate, value,        \
                                                        exception_state);      \
  }                                                                            \
  template <>                                                                  \
  CORE_EXPORT MaybeShared<T>                                                   \
  NativeValueTraits<IDLNullable<MaybeShared<T>>>::ArgumentValue(               \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value,    \
      ExceptionState& exception_state) {                                       \
    return NativeValueTraitsBufferSourceMaybeSharedArgumentValue<              \
        T, V8T, IDLBufferSourceTypeConvMode::kNullable>(                       \
        isolate, argument_index, value, exception_state);                      \
  }
#define DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_FLEXIBLE(T, V8T,      \
                                                               FLEXIBLE)    \
  template <>                                                               \
  CORE_EXPORT FLEXIBLE NativeValueTraits<FLEXIBLE>::ArgumentValue(          \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value, \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceFlexibleArgumentValue<T, V8T,       \
                                                              FLEXIBLE>(    \
        isolate, argument_index, value, exception_state);                   \
  }                                                                         \
  template <>                                                               \
  CORE_EXPORT FLEXIBLE                                                      \
  NativeValueTraits<IDLNullable<FLEXIBLE>>::ArgumentValue(                  \
      v8::Isolate* isolate, int argument_index, v8::Local<v8::Value> value, \
      ExceptionState& exception_state) {                                    \
    return NativeValueTraitsBufferSourceFlexibleArgumentValue<              \
        T, V8T, FLEXIBLE, IDLBufferSourceTypeConvMode::kNullable>(          \
        isolate, argument_index, value, exception_state);                   \
  }
#define DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(T)                           \
  DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_NOT_SHARED(DOM##T, V8##T)   \
  DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_MAYBE_SHARED(DOM##T, V8##T) \
  DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_FLEXIBLE(DOM##T, V8##T,     \
                                                         Flexible##T)
#define DEFINE_NATIVE_VALUE_TRAITS_DATA_VIEW(T)                           \
  DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_NOT_SHARED(DOM##T, V8##T) \
  DEFINE_NATIVE_VALUE_TRAITS_BUFFER_SOURCE_TYPE_MAYBE_SHARED(DOM##T, V8##T)

DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(ArrayBufferView)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Int8Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Int16Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Int32Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Uint8Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Uint8ClampedArray)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Uint16Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Uint32Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(BigInt64Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(BigUint64Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Float32Array)
DEFINE_NATIVE_VALUE_TRAITS_TYPED_ARRAY(Float64Array)
DEFINE_NATIVE_VALUE_TRAITS_DATA_VIEW(DataView)

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

// Workaround https://crbug.com/345529
XPathNSResolver* NativeValueTraits<XPathNSResolver>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<XPathNSResolver>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<IDLNullable<XPathNSResolver>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }
  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<IDLNullable<XPathNSResolver>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }
  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "XPathNSResolver"));
  return nullptr;
}

}  // namespace blink
