// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/core/page_memory.h"

#include "base/bits.h"
#include "base/memory/ptr_util.h"
#include "components/gc/core/globals.h"

namespace gc {
namespace internal {

namespace {

void Unprotect(PageAllocator* allocator, const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    CHECK(allocator->SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kReadWrite));
  } else {
    // No protection in case the allocator cannot commit at the required
    // granularity. Only protect if the allocator supports committing at that
    // granularity.
    //
    // The allocator needs to support committing the overall range.
    CHECK_EQ(0u,
             page_memory.overall_region().size() % allocator->CommitPageSize());
    CHECK(allocator->SetPermissions(page_memory.overall_region().base(),
                                    page_memory.overall_region().size(),
                                    PageAllocator::Permission::kReadWrite));
  }
}

MemoryRegion GuardMemoryRegion(const MemoryRegion overall_page_region) {
  // Always add guard pages, independently of whether they are actually
  // protected or not.
  MemoryRegion writeable_page_region(
      overall_page_region.base() + kGuardPageSize,
      overall_page_region.size() - 2 * kGuardPageSize);
  DCHECK(overall_page_region.Contains(writeable_page_region));
  return writeable_page_region;
}

MemoryRegion ReserveMemoryRegion(PageAllocator* allocator,
                                 size_t allocation_size) {
  void* region_memory =
      allocator->AllocatePages(nullptr, allocation_size, kPageSize,
                               PageAllocator::Permission::kNoAccess);
  const MemoryRegion reserved_region(static_cast<Address>(region_memory),
                                     allocation_size);
  DCHECK_EQ(reserved_region.base() + allocation_size, reserved_region.end());
  return reserved_region;
}

}  // namespace

PageMemoryRegion::PageMemoryRegion(PageAllocator* allocator,
                                   MemoryRegion reserved_region,
                                   bool is_large)
    : allocator_(allocator),
      reserved_region_(reserved_region),
      is_large_(is_large) {}

PageMemoryRegion::~PageMemoryRegion() {
  allocator_->FreePages(reserved_region().base(), reserved_region().size());
}

NormalPageMemoryRegion::NormalPageMemoryRegion(PageAllocator* allocator)
    : PageMemoryRegion(
          allocator,
          ReserveMemoryRegion(allocator,
                              base::bits::Align(kPageSize * kNumPageRegions,
                                                allocator->AllocatePageSize())),
          false) {
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    const MemoryRegion overall_page_region(
        reserved_region_.base() + i * kPageSize, kPageSize);
    DCHECK(reserved_region_.Contains(overall_page_region));
    const MemoryRegion writeable_page_region =
        GuardMemoryRegion(overall_page_region);
    page_memories_[i] = PageMemory(overall_page_region, writeable_page_region);
  }
}

NormalPageMemoryRegion::~NormalPageMemoryRegion() = default;

void NormalPageMemoryRegion::UnprotectForTesting() {
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    Unprotect(allocator_, page_memories_[i]);
  }
}

LargePageMemoryRegion::LargePageMemoryRegion(PageAllocator* allocator,
                                             size_t length)
    : PageMemoryRegion(
          allocator,
          ReserveMemoryRegion(allocator,
                              base::bits::Align(length + 2 * kGuardPageSize,
                                                allocator->AllocatePageSize())),
          true) {
  const MemoryRegion writeable_page_region =
      GuardMemoryRegion(reserved_region_);
  page_memory_ = PageMemory(reserved_region_, writeable_page_region);
}

LargePageMemoryRegion::~LargePageMemoryRegion() = default;

void LargePageMemoryRegion::UnprotectForTesting() {
  Unprotect(allocator_, page_memory_);
}

}  // namespace internal
}  // namespace gc
