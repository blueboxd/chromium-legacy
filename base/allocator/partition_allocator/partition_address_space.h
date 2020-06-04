// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace base {

namespace internal {

// The address space reservation is supported only on 64-bit architecture.
#if defined(ARCH_CPU_64_BITS)

// Reserves address space for PartitionAllocator.
class BASE_EXPORT PartitionAddressSpace {
 public:
  static PartitionAddressSpace* Instance();

  internal::pool_handle GetDirectMapPool() { return direct_map_pool_; }
  internal::pool_handle GetNormalBucketPool() { return normal_bucket_pool_; }

  void Init();
  void UninitForTesting();

  // TODO(tasak): This method should be as cheap as possible. So we can make
  // this cheaper since the range size is a power of two, but just checking that
  // the high order bits of the address are the right ones.
  bool Contains(const void* address) const {
    return reserved_address_start_ <= address &&
           address < reserved_address_end_;
  }

 private:
  // Partition Alloc Address Space
  // Reserves 32Gbytes address space for 1 direct map space(16G) and 1 normal
  // bucket space(16G).
  //
  // +----------------+ reserved address start
  // |  (unused)      |
  // +----------------+ kSuperPageSize-aligned reserved address: X
  // |                |
  // |  direct map    |
  // |    space       |
  // |                |
  // +----------------+ X + 16G bytes
  // | normal buckets |
  // |    space       |
  // +----------------+ X + 32G bytes
  // | (unused)       |
  // +----------------+ reserved address end

  static constexpr size_t kGigaBytes = static_cast<size_t>(1024 * 1024 * 1024);
  static constexpr size_t kDirectMapPoolSize =
      static_cast<size_t>(16 * kGigaBytes);
  static constexpr size_t kNormalBucketPoolSize =
      static_cast<size_t>(16 * kGigaBytes);
  // kSuperPageSize padding is added to be able to align to kSuperPageSize
  // boundary.
  static constexpr size_t kReservedAddressSpaceSize =
      kDirectMapPoolSize + kNormalBucketPoolSize + kSuperPageSize;

  char* reserved_address_start_;
  char* reserved_address_end_;

  internal::pool_handle direct_map_pool_;
  internal::pool_handle normal_bucket_pool_;
};

ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
  DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::Instance()->GetDirectMapPool();
}

ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
  DCHECK(IsPartitionAllocGigaCageEnabled());
  return PartitionAddressSpace::Instance()->GetNormalBucketPool();
}

#else  // !defined(ARCH_CPU_64_BITS)

ALWAYS_INLINE internal::pool_handle GetDirectMapPool() {
  NOTREACHED();
  return 0;
}

ALWAYS_INLINE internal::pool_handle GetNormalBucketPool() {
  NOTREACHED();
  return 0;
}

#endif

}  // namespace internal

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ADDRESS_SPACE_H_
