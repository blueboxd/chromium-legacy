// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unsanitized_atomic.h"

#include "cstdint"

#include "base/compiler_specific.h"

#if HAS_FEATURE(address_sanitizer)
#error "Must be built without asan."
#endif

namespace blink {
namespace internal {

template <typename T>
void UnsanitizedAtomic<T>::store(T value, std::memory_order order) {
  Base::store(value, order);
}

template <typename T>
T UnsanitizedAtomic<T>::load(std::memory_order order) const {
  return Base::load(order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_strong(T& expected,
                                                   T desired,
                                                   std::memory_order order) {
  return Base::compare_exchange_strong(expected, desired, order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_strong(
    T& expected,
    T desired,
    std::memory_order succ_order,
    std::memory_order fail_order) {
  return Base::compare_exchange_strong(expected, desired, succ_order,
                                       fail_order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_weak(T& expected,
                                                 T desired,
                                                 std::memory_order order) {
  return Base::compare_exchange_weak(expected, desired, order);
}

template <typename T>
bool UnsanitizedAtomic<T>::compare_exchange_weak(T& expected,
                                                 T desired,
                                                 std::memory_order succ_order,
                                                 std::memory_order fail_order) {
  return Base::compare_exchange_weak(expected, desired, succ_order, fail_order);
}

template class PLATFORM_EXPORT UnsanitizedAtomic<uint16_t>;

}  // namespace internal
}  // namespace blink
