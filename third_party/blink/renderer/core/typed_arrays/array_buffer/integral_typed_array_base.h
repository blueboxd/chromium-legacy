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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INTEGRAL_TYPED_ARRAY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INTEGRAL_TYPED_ARRAY_BASE_H_

#include <limits>
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/typed_array_base.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// Base class for all WebGL<T>Array types holding integral
// (non-floating-point) values.
template <typename T, bool clamped = false>
class IntegralTypedArrayBase : public TypedArrayBase<T> {
 public:
  static inline scoped_refptr<IntegralTypedArrayBase<T, clamped>> Create(
      unsigned length);
  static inline scoped_refptr<IntegralTypedArrayBase<T, clamped>> Create(
      const T* array,
      unsigned length);
  static inline scoped_refptr<IntegralTypedArrayBase<T, clamped>>
  Create(scoped_refptr<ArrayBuffer>, unsigned byte_offset, unsigned length);

  inline void Set(unsigned index, double value);

  inline void Set(unsigned index, uint64_t value);

  ArrayBufferView::ViewType GetType() const override;

  IntegralTypedArrayBase(scoped_refptr<ArrayBuffer> buffer,
                         unsigned byte_offset,
                         unsigned length)
      : TypedArrayBase<T>(std::move(buffer), byte_offset, length) {}
};

template <typename T, bool clamped>
scoped_refptr<IntegralTypedArrayBase<T, clamped>>
IntegralTypedArrayBase<T, clamped>::Create(unsigned length) {
  return TypedArrayBase<T>::template Create<IntegralTypedArrayBase<T, clamped>>(
      length);
}

template <typename T, bool clamped>
scoped_refptr<IntegralTypedArrayBase<T, clamped>>
IntegralTypedArrayBase<T, clamped>::Create(const T* array, unsigned length) {
  return TypedArrayBase<T>::template Create<IntegralTypedArrayBase<T, clamped>>(
      array, length);
}

template <typename T, bool clamped>
scoped_refptr<IntegralTypedArrayBase<T, clamped>>
IntegralTypedArrayBase<T, clamped>::Create(scoped_refptr<ArrayBuffer> buffer,
                                           unsigned byte_offset,
                                           unsigned length) {
  return TypedArrayBase<T>::template Create<IntegralTypedArrayBase<T, clamped>>(
      std::move(buffer), byte_offset, length);
}

template <typename T, bool clamped>
inline void IntegralTypedArrayBase<T, clamped>::Set(unsigned index,
                                                    double value) {
  if (index >= TypedArrayBase<T>::length_)
    return;
  if (std::isnan(value))  // Clamp NaN to 0
    value = 0;
  // The double cast is necessary to get the correct wrapping
  // for out-of-range values with Int32Array and Uint32Array.
  TypedArrayBase<T>::Data()[index] =
      static_cast<T>(static_cast<int64_t>(value));
}

template <>
inline void IntegralTypedArrayBase<uint8_t, true>::Set(unsigned index,
                                                       double value) {
  if (index >= TypedArrayBase<uint8_t>::length_) {
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
inline void IntegralTypedArrayBase<float, false>::Set(unsigned index,
                                                      double value) {
  if (index >= TypedArrayBase<float>::length_)
    return;
  TypedArrayBase<float>::Data()[index] = static_cast<float>(value);
}

template <>
inline void IntegralTypedArrayBase<double, false>::Set(unsigned index,
                                                       double value) {
  if (index >= TypedArrayBase<double>::length_)
    return;
  TypedArrayBase<double>::Data()[index] = value;
}

template <typename T, bool clamped>
inline void IntegralTypedArrayBase<T, clamped>::Set(unsigned index,
                                                    uint64_t value) {
  NOTREACHED();
}

template <>
inline void IntegralTypedArrayBase<int64_t, false>::Set(unsigned index,
                                                        uint64_t value) {
  if (index >= TypedArrayBase<int64_t>::length_)
    return;
  TypedArrayBase<int64_t>::Data()[index] = static_cast<int64_t>(value);
}

template <>
inline void IntegralTypedArrayBase<uint64_t, false>::Set(unsigned index,
                                                         uint64_t value) {
  if (index >= TypedArrayBase<uint64_t>::length_)
    return;
  TypedArrayBase<uint64_t>::Data()[index] = value;
}

template <typename T, bool clamped>
inline ArrayBufferView::ViewType IntegralTypedArrayBase<T, clamped>::GetType()
    const {
  NOTREACHED();
  return ArrayBufferView::kTypeInt16;
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

#define GET_TYPE(c_type, view_type)                        \
  template <>                                              \
  inline ArrayBufferView::ViewType                         \
  IntegralTypedArrayBase<c_type, false>::GetType() const { \
    return ArrayBufferView::view_type;                     \
  }

FOREACH_VIEW_TYPE(GET_TYPE)

#undef GET_TYPE
#undef FOREACH_VIEW_TYPE

template <>
inline ArrayBufferView::ViewType
IntegralTypedArrayBase<uint8_t, true>::GetType() const {
  return ArrayBufferView::kTypeUint8Clamped;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INTEGRAL_TYPED_ARRAY_BASE_H_
