// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

#include "base/allocator/partition_allocator/partition_alloc.h"

namespace base {
namespace internal {

#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

BASE_EXPORT bool CheckedPtr2ImplPartitionAllocSupport::EnabledForPtr(
    void* ptr) {
  // CheckedPtr2Impl works only when memory is allocated by PartitionAlloc *and*
  // the pointer points to the beginning of the allocated slot.
  //
  // NOTE, CheckedPtr doesn't know which thread-safery PartitionAlloc variant
  // it's dealing with. Just use ThreadSafe variant, because it's more common.
  // NotThreadSafe is only used by Blink's layout, which is currently being
  // transitioned to Oilpan. PartitionAllocGetSlotOffset is expected to return
  // the same result regardless, anyway.
  // TODO(bartekn): Figure out the thread-safety mismatch.
  return IsManagedByPartitionAllocAndNotDirectMapped(ptr) &&
         PartitionAllocGetSlotOffset<ThreadSafe>(ptr) == 0;
}

#endif

}  // namespace internal
}  // namespace base
