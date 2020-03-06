// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/core/page_memory.h"

#include "base/bits.h"
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

void Protect(PageAllocator* allocator, const PageMemory& page_memory) {
  if (SupportsCommittingGuardPages(allocator)) {
    // Swap the same region, providing the OS with a chance for fast lookup and
    // change.
    CHECK(allocator->SetPermissions(page_memory.writeable_region().base(),
                                    page_memory.writeable_region().size(),
                                    PageAllocator::Permission::kNoAccess));
  } else {
    // See Unprotect().
    CHECK_EQ(0u,
             page_memory.overall_region().size() % allocator->CommitPageSize());
    CHECK(allocator->SetPermissions(page_memory.overall_region().base(),
                                    page_memory.overall_region().size(),
                                    PageAllocator::Permission::kNoAccess));
  }
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

void FreeMemoryRegion(PageAllocator* allocator,
                      const MemoryRegion& reserved_region) {
  allocator->FreePages(reserved_region.base(), reserved_region.size());
}

}  // namespace

PageMemoryRegion::PageMemoryRegion(PageAllocator* allocator,
                                   MemoryRegion reserved_region,
                                   bool is_large)
    : allocator_(allocator),
      reserved_region_(reserved_region),
      is_large_(is_large) {}

PageMemoryRegion::~PageMemoryRegion() {
  FreeMemoryRegion(allocator_, reserved_region());
}

// static
constexpr size_t NormalPageMemoryRegion::kNumPageRegions;

NormalPageMemoryRegion::NormalPageMemoryRegion(PageAllocator* allocator)
    : PageMemoryRegion(
          allocator,
          ReserveMemoryRegion(allocator,
                              base::bits::Align(kPageSize * kNumPageRegions,
                                                allocator->AllocatePageSize())),
          false) {
#ifdef DEBUG
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    DCHECK_EQ(false, page_memories_in_use_[i]);
  }
#endif  // DEBUG
}

NormalPageMemoryRegion::~NormalPageMemoryRegion() = default;

void NormalPageMemoryRegion::Allocate(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  ChangeUsed(index, true);
  Unprotect(allocator_, GetPageMemory(index));
}

void NormalPageMemoryRegion::Free(Address writeable_base) {
  const size_t index = GetIndex(writeable_base);
  ChangeUsed(index, false);
  Protect(allocator_, GetPageMemory(index));
}

void NormalPageMemoryRegion::UnprotectForTesting() {
  for (size_t i = 0; i < kNumPageRegions; ++i) {
    Unprotect(allocator_, GetPageMemory(i));
  }
}

LargePageMemoryRegion::LargePageMemoryRegion(PageAllocator* allocator,
                                             size_t length)
    : PageMemoryRegion(
          allocator,
          ReserveMemoryRegion(allocator,
                              base::bits::Align(length + 2 * kGuardPageSize,
                                                allocator->AllocatePageSize())),
          true) {}

LargePageMemoryRegion::~LargePageMemoryRegion() = default;

void LargePageMemoryRegion::UnprotectForTesting() {
  Unprotect(allocator_, GetPageMemory());
}

PageMemoryRegionTree::PageMemoryRegionTree() = default;

PageMemoryRegionTree::~PageMemoryRegionTree() = default;

void PageMemoryRegionTree::Add(PageMemoryRegion* region) {
  DCHECK(region);
  auto result = set_.emplace(region->reserved_region().base(), region);
  DCHECK(result.second);
}

void PageMemoryRegionTree::Remove(PageMemoryRegion* region) {
  DCHECK(region);
  auto size = set_.erase(region->reserved_region().base());
  DCHECK_EQ(1u, size);
}

NormalPageMemoryPool::NormalPageMemoryPool() = default;

NormalPageMemoryPool::~NormalPageMemoryPool() = default;

void NormalPageMemoryPool::Add(Address writeable_base) {
  pool_.push_back(writeable_base);
}

Address NormalPageMemoryPool::Take() {
  if (pool_.empty())
    return nullptr;
  Address writeable_base = pool_.back();
  pool_.pop_back();
  return writeable_base;
}

PageBackend::PageBackend(PageAllocator* allocator) : allocator_(allocator) {}

PageBackend::~PageBackend() = default;

Address PageBackend::AllocateNormalPageMemory() {
  Address writeable_base = page_pool_.Take();
  if (!writeable_base) {
    auto pmr = std::make_unique<NormalPageMemoryRegion>(allocator_);
    for (size_t i = 0; i < NormalPageMemoryRegion::kNumPageRegions; ++i) {
      page_pool_.Add(pmr->GetPageMemory(i).writeable_region().base());
    }
    page_memory_region_tree_.Add(pmr.get());
    normal_page_memory_regions_.push_back(std::move(pmr));
    return AllocateNormalPageMemory();
  }
  static_cast<NormalPageMemoryRegion*>(
      page_memory_region_tree_.Lookup(writeable_base))
      ->Allocate(writeable_base);
  return writeable_base;
}

void PageBackend::FreeNormalPageMemory(Address writeable_base) {
  static_cast<NormalPageMemoryRegion*>(
      page_memory_region_tree_.Lookup(writeable_base))
      ->Free(writeable_base);
  page_pool_.Add(writeable_base);
}

Address PageBackend::AllocateLargePageMemory(size_t size) {
  auto pmr = std::make_unique<LargePageMemoryRegion>(allocator_, size);
  const PageMemory pm = pmr->GetPageMemory();
  Unprotect(allocator_, pm);
  page_memory_region_tree_.Add(pmr.get());
  large_page_memory_regions_.insert({pmr.get(), std::move(pmr)});
  return pm.writeable_region().base();
}

void PageBackend::FreeLargePageMemory(Address writeable_base) {
  PageMemoryRegion* pmr = page_memory_region_tree_.Lookup(writeable_base);
  page_memory_region_tree_.Remove(pmr);
  auto size = large_page_memory_regions_.erase(pmr);
  DCHECK_EQ(1u, size);
}

}  // namespace internal
}  // namespace gc
