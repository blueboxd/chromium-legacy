// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/test/base_allocator.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"

namespace gc {
namespace test {

namespace {

base::PageAccessibilityConfiguration GetPageAccessibility(
    PageAllocator::Permission permission) {
  switch (permission) {
    case PageAllocator::Permission::kRead:
      return base::PageRead;
    case PageAllocator::Permission::kReadWrite:
      return base::PageReadWrite;
    case PageAllocator::Permission::kReadExecute:
      return base::PageReadExecute;
    case PageAllocator::Permission::kNoAccess:
      return base::PageInaccessible;
  }
}

}  // namespace

size_t BaseAllocator::AllocatePageSize() const {
  return base::kPageAllocationGranularity;
}

size_t BaseAllocator::CommitPageSize() const {
  return base::kSystemPageSize;
}

void* BaseAllocator::AllocatePages(void* address,
                                   size_t length,
                                   size_t alignment,
                                   Permission permissions) {
  base::PageAccessibilityConfiguration config =
      GetPageAccessibility(permissions);
  const bool commit = (permissions != PageAllocator::Permission::kNoAccess);
  // Use generic PartitionAlloc page tag as the allocator is only used for
  // testing.
  const base::PageTag page_tag = base::PageTag::kChromium;
  return base::AllocPages(address, length, alignment, config, page_tag, commit);
}

bool BaseAllocator::FreePages(void* address, size_t length) {
  base::FreePages(address, length);
  return true;
}

// Sets permissions in a range that was allocated by |AllocatedPages()|.
bool BaseAllocator::SetPermissions(void* address,
                                   size_t length,
                                   Permission permissions) {
  if (permissions == Permission::kNoAccess) {
    base::DecommitSystemPages(address, length);
    return true;
  }
  return base::TrySetSystemPagesAccess(address, length,
                                       GetPageAccessibility(permissions));
}

bool BaseAllocator::DiscardSystemPages(void* address, size_t size) {
  return false;
}

}  // namespace test
}  // namespace gc
