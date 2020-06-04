// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bits.h"

namespace base {

namespace internal {

#if defined(ARCH_CPU_64_BITS)

constexpr size_t PartitionAddressSpace::kGigaBytes;
constexpr size_t PartitionAddressSpace::kDirectMapPoolSize;
constexpr size_t PartitionAddressSpace::kNormalBucketPoolSize;
constexpr size_t PartitionAddressSpace::kReservedAddressSpaceSize;

// static
PartitionAddressSpace* PartitionAddressSpace::Instance() {
  static NoDestructor<PartitionAddressSpace> instance;
  return instance.get();
}

void PartitionAddressSpace::Init() {
  reserved_address_start_ = reinterpret_cast<char*>(SystemAllocPages(
      nullptr, kReservedAddressSpaceSize, base::PageInaccessible,
      PageTag::kPartitionAlloc, false));
  DCHECK(reserved_address_start_);
  reserved_address_end_ = reserved_address_start_ + kReservedAddressSpaceSize;

  char* current = reinterpret_cast<char*>(bits::Align(
      reinterpret_cast<uintptr_t>(reserved_address_start_), kSuperPageSize));
  DCHECK_GE(current, reserved_address_start_);
  DCHECK(!(reinterpret_cast<uintptr_t>(current) & kSuperPageOffsetMask));

  direct_map_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      current, kDirectMapPoolSize, kSuperPageSize);
  DCHECK(direct_map_pool_);
  current += kDirectMapPoolSize;

  normal_bucket_pool_ = internal::AddressPoolManager::GetInstance()->Add(
      current, kNormalBucketPoolSize, kSuperPageSize);
  DCHECK(normal_bucket_pool_);
  current += kNormalBucketPoolSize;
  DCHECK_LE(current, reserved_address_end_);
}

void PartitionAddressSpace::UninitForTesting() {
  DCHECK(reserved_address_start_);
  DCHECK(reserved_address_end_);
  FreePages(reserved_address_start_,
            reserved_address_end_ - reserved_address_start_);
  reserved_address_start_ = nullptr;
  reserved_address_end_ = nullptr;
  direct_map_pool_ = 0;
  internal::AddressPoolManager::GetInstance()->ResetForTesting();
}

#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace internal

}  // namespace base
