// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GC_CORE_PAGE_MEMORY_H_
#define COMPONENTS_GC_CORE_PAGE_MEMORY_H_

#include <array>
#include <memory>

#include "base/logging.h"
#include "components/gc/core/gc_export.h"
#include "components/gc/core/globals.h"
#include "components/gc/public/platform.h"

namespace gc {
namespace internal {

// Returns true if the provided allocator supports committing at the required
// granularity.
inline bool SupportsCommittingGuardPages(PageAllocator* allocator) {
  return kGuardPageSize % allocator->CommitPageSize() == 0;
}

class GC_EXPORT MemoryRegion final {
 public:
  MemoryRegion() = default;
  MemoryRegion(Address base, size_t size) : base_(base), size_(size) {
    DCHECK(base);
    DCHECK_LT(0u, size);
  }

  Address base() const { return base_; }
  size_t size() const { return size_; }
  Address end() const { return base_ + size_; }

  bool Contains(Address addr) const {
    return (reinterpret_cast<uintptr_t>(addr) -
            reinterpret_cast<uintptr_t>(base_)) < size_;
  }

  bool Contains(const MemoryRegion& other) const {
    return base_ <= other.base() && other.end() <= end();
  }

 private:
  Address base_ = nullptr;
  size_t size_ = 0;
};

// PageMemory provides the backing of a single normal or large page.
class GC_EXPORT PageMemory final {
 public:
  PageMemory() = default;
  PageMemory(MemoryRegion overall, MemoryRegion writable)
      : overall_(overall), writable_(writable) {
    DCHECK(overall.Contains(writable));
  }

  const MemoryRegion overall_region() const { return overall_; }
  const MemoryRegion writeable_region() const { return writable_; }

 private:
  MemoryRegion overall_;
  MemoryRegion writable_;
};

class GC_EXPORT PageMemoryRegion {
 public:
  virtual ~PageMemoryRegion();

  MemoryRegion reserved_region() const { return reserved_region_; }

  bool is_large() const { return is_large_; }

  // Disallow copy.
  PageMemoryRegion(const PageMemoryRegion&) = delete;
  PageMemoryRegion& operator=(const PageMemoryRegion&) = delete;

  virtual void UnprotectForTesting() = 0;

 protected:
  PageMemoryRegion(PageAllocator*, MemoryRegion, bool);

  PageAllocator* const allocator_;
  const MemoryRegion reserved_region_;
  const bool is_large_;
};

// NormalPageMemoryRegion serves kNumPageRegions normal-sized PageMemory object.
class GC_EXPORT NormalPageMemoryRegion final : public PageMemoryRegion {
 public:
  static constexpr size_t kNumPageRegions = 10;

  explicit NormalPageMemoryRegion(PageAllocator*);
  ~NormalPageMemoryRegion() override;

  const PageMemory* begin() { return page_memories_.cbegin(); }
  const PageMemory* end() { return page_memories_.cend(); }

  void UnprotectForTesting() final;

 private:
  std::array<PageMemory, kNumPageRegions> page_memories_ = {};
};

// LargePageMemoryRegion serves a single large PageMemory object.
class GC_EXPORT LargePageMemoryRegion final : public PageMemoryRegion {
 public:
  LargePageMemoryRegion(PageAllocator*, size_t);
  ~LargePageMemoryRegion() override;

  const PageMemory* page_memory() const { return &page_memory_; }

  void UnprotectForTesting() final;

 private:
  PageMemory page_memory_;
};

}  // namespace internal
}  // namespace gc

#endif  // COMPONENTS_GC_CORE_PAGE_MEMORY_H_
