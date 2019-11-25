/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_H_

#include <limits>
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_view.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

template <typename T, bool clamped = false>
class TypedArray : public ArrayBufferView {
 public:
  typedef T ValueType;

  static inline scoped_refptr<TypedArray<T, clamped>> Create(size_t length);
  static inline scoped_refptr<TypedArray<T, clamped>> Create(const T* array,
                                                             size_t length);
  static inline scoped_refptr<TypedArray<T, clamped>>
  Create(scoped_refptr<ArrayBuffer>, size_t byte_offset, size_t length);

  T* Data() const { return static_cast<T*>(BaseAddress()); }

  T* DataMaybeShared() const {
    return static_cast<T*>(BaseAddressMaybeShared());
  }

  size_t length() const { return length_; }

  size_t ByteLengthAsSizeT() const final { return length_ * sizeof(T); }

  unsigned TypeSize() const final { return sizeof(T); }

  inline void Set(size_t index, double value);

  inline void Set(size_t index, uint64_t value);

  ArrayBufferView::ViewType GetType() const override;

  TypedArray(scoped_refptr<ArrayBuffer> buffer,
             size_t byte_offset,
             size_t length)
      : ArrayBufferView(std::move(buffer), byte_offset), length_(length) {}

  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  T Item(size_t index) const {
    SECURITY_DCHECK(index < length_);
    return Data()[index];
  }

 private:
  void Detach() final {
    ArrayBufferView::Detach();
    length_ = 0;
  }

  size_t length_;
};

template <typename T, bool clamped>
scoped_refptr<TypedArray<T, clamped>> TypedArray<T, clamped>::Create(
    size_t length) {
  scoped_refptr<ArrayBuffer> buffer = ArrayBuffer::Create(length, sizeof(T));
  return Create(std::move(buffer), 0, length);
}

template <typename T, bool clamped>
scoped_refptr<TypedArray<T, clamped>> TypedArray<T, clamped>::Create(
    const T* array,
    size_t length) {
  auto a = Create(length);
  if (a) {
    std::memcpy(a->Data(), array, a->ByteLengthAsSizeT());
  }
  return a;
}

namespace {
// Helper to verify that a given sub-range of an ArrayBuffer is within range.
template <typename T>
bool VerifySubRange(const ArrayBuffer* buffer,
                    size_t byte_offset,
                    size_t num_elements) {
  if (!buffer)
    return false;
  if (sizeof(T) > 1 && byte_offset % sizeof(T))
    return false;
  if (byte_offset > buffer->ByteLengthAsSizeT())
    return false;
  size_t remaining_elements =
      (buffer->ByteLengthAsSizeT() - byte_offset) / sizeof(T);
  if (num_elements > remaining_elements)
    return false;
  return true;
}
}  // namespace

template <typename T, bool clamped>
scoped_refptr<TypedArray<T, clamped>> TypedArray<T, clamped>::Create(
    scoped_refptr<ArrayBuffer> buffer,
    size_t byte_offset,
    size_t length) {
  CHECK(VerifySubRange<T>(buffer.get(), byte_offset, length));
  return base::AdoptRef(
      new TypedArray<T, clamped>(std::move(buffer), byte_offset, length));
}

template <typename T, bool clamped>
inline void TypedArray<T, clamped>::Set(size_t index, double value) {
  if (index >= length_)
    return;
  if (std::isnan(value))  // Clamp NaN to 0
    value = 0;
  // The double cast is necessary to get the correct wrapping
  // for out-of-range values with Int32Array and Uint32Array.
  Data()[index] = static_cast<T>(static_cast<int64_t>(value));
}

template <>
inline void TypedArray<uint8_t, true>::Set(size_t index, double value) {
  if (index >= length_) {
    return;
  }
  if (std::isnan(value) || value < 0) {
    value = 0;
  } else if (value > 255) {
    value = 255;
  }

  Data()[index] = static_cast<unsigned char>(lrint(value));
}

template <>
inline void TypedArray<float, false>::Set(size_t index, double value) {
  if (index >= length_)
    return;
  Data()[index] = static_cast<float>(value);
}

template <>
inline void TypedArray<double, false>::Set(size_t index, double value) {
  if (index >= length_)
    return;
  Data()[index] = value;
}

template <>
inline void TypedArray<int64_t, false>::Set(size_t index, uint64_t value) {
  if (index >= length_)
    return;
  Data()[index] = static_cast<int64_t>(value);
}

template <>
inline void TypedArray<uint64_t, false>::Set(size_t index, uint64_t value) {
  if (index >= length_)
    return;
  Data()[index] = value;
}

template <>
inline void TypedArray<int64_t, false>::Set(size_t index, double value) {
  // This version of {Set} is not supposed to be used for a TypedArray of type
  // int64_t.
  NOTREACHED();
}

template <>
inline void TypedArray<uint64_t, false>::Set(size_t index, double value) {
  // This version of {Set} is not supposed to be used for a TypedArray of type
  // uint64_t.
  NOTREACHED();
}

template <typename T, bool clamped>
inline void TypedArray<T, clamped>::Set(size_t index, uint64_t value) {
  // This version of {Set} is only supposed to be used for a TypedArrays of type
  // int64_t or uint64_t.
  NOTREACHED();
}

#define FOREACH_VIEW_TYPE(V) \
  V(int8_t, kTypeInt8)       \
  V(int16_t, kTypeInt16)     \
  V(int32_t, kTypeInt32)     \
  V(uint8_t, kTypeUint8)     \
  V(uint16_t, kTypeUint16)   \
  V(uint32_t, kTypeUint32)   \
  V(float, kTypeFloat32)     \
  V(double, kTypeFloat64)    \
  V(int64_t, kTypeBigInt64)  \
  V(uint64_t, kTypeBigUint64)

#define GET_TYPE(c_type, view_type)                                     \
  template <>                                                           \
  inline ArrayBufferView::ViewType TypedArray<c_type, false>::GetType() \
      const {                                                           \
    return ArrayBufferView::view_type;                                  \
  }

FOREACH_VIEW_TYPE(GET_TYPE)

#undef GET_TYPE
#undef FOREACH_VIEW_TYPE

template <>
inline ArrayBufferView::ViewType TypedArray<uint8_t, true>::GetType() const {
  return ArrayBufferView::kTypeUint8Clamped;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_H_
