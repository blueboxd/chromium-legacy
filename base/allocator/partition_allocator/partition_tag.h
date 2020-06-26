// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/notreached.h"
#include "build/build_config.h"

#define ENABLE_CHECKED_PTR 0

namespace base {
namespace internal {

// Use 16 bits for the partition tag.
// TODO(tasak): add a description about the partition tag.
using PartitionTag = uint16_t;

static constexpr PartitionTag kTagTemporaryInitialValue = 0x0BAD;

#if ENABLE_CHECKED_PTR

// Allocate extra 16 bytes for the partition tag. 14 bytes are unused
// (reserved).
static constexpr size_t kPartitionTagSize = 16;

#if DCHECK_IS_ON()
// The layout inside the slot is |tag|cookie|object|(empty)|cookie|.
static constexpr size_t kPartitionTagOffset = kPartitionTagSize + kCookieSize;
#else
// The layout inside the slot is |tag|object|(empty)|.
static constexpr size_t kPartitionTagOffset = kPartitionTagSize;
#endif

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  PA_DCHECK(size + kPartitionTagSize > size);
  return size + kPartitionTagSize;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  PA_DCHECK(size >= kPartitionTagSize);
  return size - kPartitionTagSize;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  return reinterpret_cast<PartitionTag*>(reinterpret_cast<char*>(ptr) -
                                         kPartitionTagOffset);
}

ALWAYS_INLINE void* PartitionTagFreePointerAdjust(void* ptr) {
  return reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) -
                                 kPartitionTagSize);
}

ALWAYS_INLINE void PartitionTagSetValue(void* ptr, uint16_t value) {
  *PartitionTagPointer(ptr) = value;
}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void* ptr) {
  return *PartitionTagPointer(ptr);
}

#else  // !ENABLE_CHECKED_PTR

// No tag added.
static constexpr size_t kPartitionTagSize = 0;

ALWAYS_INLINE size_t PartitionTagSizeAdjustAdd(size_t size) {
  return size;
}

ALWAYS_INLINE size_t PartitionTagSizeAdjustSubtract(size_t size) {
  return size;
}

ALWAYS_INLINE PartitionTag* PartitionTagPointer(void* ptr) {
  NOTREACHED();
  return nullptr;
}

ALWAYS_INLINE void* PartitionTagFreePointerAdjust(void* ptr) {
  return ptr;
}

ALWAYS_INLINE void PartitionTagSetValue(void*, uint16_t) {}

ALWAYS_INLINE PartitionTag PartitionTagGetValue(void*) {
  return 0;
}

#endif  // !ENABLE_CHECKED_PTR

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_TAG_H_
