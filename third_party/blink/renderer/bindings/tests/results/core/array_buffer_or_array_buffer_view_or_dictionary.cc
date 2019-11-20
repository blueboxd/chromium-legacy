// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/array_buffer_or_array_buffer_view_or_dictionary.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"

namespace blink {

ArrayBufferOrArrayBufferViewOrDictionary::ArrayBufferOrArrayBufferViewOrDictionary() : type_(SpecificType::kNone) {}

TestArrayBuffer* ArrayBufferOrArrayBufferViewOrDictionary::GetAsArrayBuffer() const {
  DCHECK(IsArrayBuffer());
  return array_buffer_;
}

void ArrayBufferOrArrayBufferViewOrDictionary::SetArrayBuffer(TestArrayBuffer* value) {
  DCHECK(IsNull());
  array_buffer_ = value;
  type_ = SpecificType::kArrayBuffer;
}

ArrayBufferOrArrayBufferViewOrDictionary ArrayBufferOrArrayBufferViewOrDictionary::FromArrayBuffer(TestArrayBuffer* value) {
  ArrayBufferOrArrayBufferViewOrDictionary container;
  container.SetArrayBuffer(value);
  return container;
}

NotShared<TestArrayBufferView> ArrayBufferOrArrayBufferViewOrDictionary::GetAsArrayBufferView() const {
  DCHECK(IsArrayBufferView());
  return array_buffer_view_;
}

void ArrayBufferOrArrayBufferViewOrDictionary::SetArrayBufferView(NotShared<TestArrayBufferView> value) {
  DCHECK(IsNull());
  array_buffer_view_ = Member<TestArrayBufferView>(value.View());
  type_ = SpecificType::kArrayBufferView;
}

ArrayBufferOrArrayBufferViewOrDictionary ArrayBufferOrArrayBufferViewOrDictionary::FromArrayBufferView(NotShared<TestArrayBufferView> value) {
  ArrayBufferOrArrayBufferViewOrDictionary container;
  container.SetArrayBufferView(value);
  return container;
}

Dictionary ArrayBufferOrArrayBufferViewOrDictionary::GetAsDictionary() const {
  DCHECK(IsDictionary());
  return dictionary_;
}

void ArrayBufferOrArrayBufferViewOrDictionary::SetDictionary(Dictionary value) {
  DCHECK(IsNull());
  dictionary_ = value;
  type_ = SpecificType::kDictionary;
}

ArrayBufferOrArrayBufferViewOrDictionary ArrayBufferOrArrayBufferViewOrDictionary::FromDictionary(Dictionary value) {
  ArrayBufferOrArrayBufferViewOrDictionary container;
  container.SetDictionary(value);
  return container;
}

ArrayBufferOrArrayBufferViewOrDictionary::ArrayBufferOrArrayBufferViewOrDictionary(const ArrayBufferOrArrayBufferViewOrDictionary&) = default;
ArrayBufferOrArrayBufferViewOrDictionary::~ArrayBufferOrArrayBufferViewOrDictionary() = default;
ArrayBufferOrArrayBufferViewOrDictionary& ArrayBufferOrArrayBufferViewOrDictionary::operator=(const ArrayBufferOrArrayBufferViewOrDictionary&) = default;

void ArrayBufferOrArrayBufferViewOrDictionary::Trace(blink::Visitor* visitor) {
  visitor->Trace(array_buffer_);
  visitor->Trace(array_buffer_view_);
}

void V8ArrayBufferOrArrayBufferViewOrDictionary::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    ArrayBufferOrArrayBufferViewOrDictionary& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (v8_value->IsArrayBuffer()) {
    TestArrayBuffer* cpp_value = V8ArrayBuffer::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetArrayBuffer(cpp_value);
    return;
  }

  if (v8_value->IsArrayBufferView()) {
    NotShared<TestArrayBufferView> cpp_value = ToNotShared<NotShared<TestArrayBufferView>>(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetArrayBufferView(cpp_value);
    return;
  }

  if (IsUndefinedOrNull(v8_value) || v8_value->IsObject()) {
    Dictionary cpp_value = NativeValueTraits<Dictionary>::NativeValue(isolate, v8_value, exception_state);
    if (exception_state.HadException())
      return;
    impl.SetDictionary(cpp_value);
    return;
  }

  exception_state.ThrowTypeError("The provided value is not of type '(ArrayBuffer or ArrayBufferView or Dictionary)'");
}

v8::Local<v8::Value> ToV8(const ArrayBufferOrArrayBufferViewOrDictionary& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case ArrayBufferOrArrayBufferViewOrDictionary::SpecificType::kNone:
      return v8::Null(isolate);
    case ArrayBufferOrArrayBufferViewOrDictionary::SpecificType::kArrayBuffer:
      return ToV8(impl.GetAsArrayBuffer(), creationContext, isolate);
    case ArrayBufferOrArrayBufferViewOrDictionary::SpecificType::kArrayBufferView:
      return ToV8(impl.GetAsArrayBufferView(), creationContext, isolate);
    case ArrayBufferOrArrayBufferViewOrDictionary::SpecificType::kDictionary:
      return impl.GetAsDictionary().V8Value();
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

ArrayBufferOrArrayBufferViewOrDictionary NativeValueTraits<ArrayBufferOrArrayBufferViewOrDictionary>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  ArrayBufferOrArrayBufferViewOrDictionary impl;
  V8ArrayBufferOrArrayBufferViewOrDictionary::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

