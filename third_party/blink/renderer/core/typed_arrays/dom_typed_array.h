// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/typed_array.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

template <typename TypedArray, typename V8TypedArray>
class DOMTypedArray final : public DOMArrayBufferView {
  typedef DOMTypedArray<TypedArray, V8TypedArray> ThisType;
  DECLARE_WRAPPERTYPEINFO();

 public:
  typedef typename TypedArray::ValueType ValueType;

  static ThisType* Create(scoped_refptr<TypedArray> buffer_view) {
    return MakeGarbageCollected<ThisType>(std::move(buffer_view));
  }
  static ThisType* Create(size_t length) {
    return Create(TypedArray::Create(length));
  }
  static ThisType* Create(const ValueType* array, size_t length) {
    return Create(TypedArray::Create(array, length));
  }
  static ThisType* Create(scoped_refptr<ArrayBuffer> buffer,
                          size_t byte_offset,
                          size_t length) {
    return Create(TypedArray::Create(std::move(buffer), byte_offset, length));
  }
  static ThisType* Create(DOMArrayBufferBase* buffer,
                          size_t byte_offset,
                          size_t length) {
    scoped_refptr<TypedArray> buffer_view =
        TypedArray::Create(buffer->Buffer(), byte_offset, length);
    return MakeGarbageCollected<ThisType>(std::move(buffer_view), buffer);
  }

  static ThisType* CreateOrNull(size_t length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  static ThisType* CreateUninitializedOrNull(size_t length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  explicit DOMTypedArray(scoped_refptr<TypedArray> buffer_view)
      : DOMArrayBufferView(std::move(buffer_view)) {}
  DOMTypedArray(scoped_refptr<TypedArray> buffer_view,
                DOMArrayBufferBase* dom_array_buffer)
      : DOMArrayBufferView(std::move(buffer_view), dom_array_buffer) {}

  const TypedArray* View() const {
    return static_cast<const TypedArray*>(DOMArrayBufferView::View());
  }
  TypedArray* View() {
    return static_cast<TypedArray*>(DOMArrayBufferView::View());
  }

  ValueType* Data() const { return View()->Data(); }
  ValueType* DataMaybeShared() const { return View()->DataMaybeShared(); }
  size_t lengthAsSizeT() const { return View()->length(); }
  // This function is deprecated and should not be used. Use {lengthAsSizeT}
  // instead.
  unsigned deprecatedLengthAsUnsigned() const {
    return base::checked_cast<unsigned>(View()->length());
  }
  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  ValueType Item(size_t index) const { return View()->Item(index); }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<int8_t>, v8::Int8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<int16_t>, v8::Int16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<int32_t>, v8::Int32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<uint8_t>, v8::Uint8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<uint8_t, /*clamped=*/true>, v8::Uint8ClampedArray>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<uint16_t>, v8::Uint16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<uint32_t>, v8::Uint32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<int64_t>, v8::BigInt64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<uint64_t>, v8::BigUint64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<float>, v8::Float32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<TypedArray<double>, v8::Float64Array>;

typedef DOMTypedArray<TypedArray<int8_t>, v8::Int8Array> DOMInt8Array;
typedef DOMTypedArray<TypedArray<int16_t>, v8::Int16Array> DOMInt16Array;
typedef DOMTypedArray<TypedArray<int32_t>, v8::Int32Array> DOMInt32Array;
typedef DOMTypedArray<TypedArray<uint8_t>, v8::Uint8Array> DOMUint8Array;
typedef DOMTypedArray<TypedArray<uint8_t, /*clamped=*/true>,
                      v8::Uint8ClampedArray>
    DOMUint8ClampedArray;
typedef DOMTypedArray<TypedArray<uint16_t>, v8::Uint16Array> DOMUint16Array;
typedef DOMTypedArray<TypedArray<uint32_t>, v8::Uint32Array> DOMUint32Array;
typedef DOMTypedArray<TypedArray<int64_t>, v8::BigInt64Array> DOMBigInt64Array;
typedef DOMTypedArray<TypedArray<uint64_t>, v8::BigUint64Array>
    DOMBigUint64Array;
typedef DOMTypedArray<TypedArray<float>, v8::Float32Array> DOMFloat32Array;
typedef DOMTypedArray<TypedArray<double>, v8::Float64Array> DOMFloat64Array;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
