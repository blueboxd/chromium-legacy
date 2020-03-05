// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/core/page_memory.h"

#include "build/build_config.h"
#include "components/gc/test/base_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gc {
namespace internal {
namespace test {

TEST(PageMemoryRegionTest, NormalPageMemoryRegion) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  constexpr size_t kExpectedPageMemories = 10;
  size_t page_memory_cnt = 0;
  MemoryRegion prev_overall;
  for (auto& pm : *pmr) {
    page_memory_cnt++;
    // Previous PageMemory aligns with the current one.
    if (prev_overall.base()) {
      EXPECT_EQ(prev_overall.end(), pm.overall_region().base());
    }
    prev_overall =
        MemoryRegion(pm.overall_region().base(), pm.overall_region().size());
    // Writeable region is contained in overall region.
    EXPECT_TRUE(pm.overall_region().Contains(pm.writeable_region()));

    // Front guard page.
    EXPECT_EQ(pm.writeable_region().base(),
              pm.overall_region().base() + kGuardPageSize);
    // Back guard page.
    EXPECT_EQ(pm.overall_region().end(),
              pm.writeable_region().end() + kGuardPageSize);
  }
  EXPECT_EQ(kExpectedPageMemories, page_memory_cnt);
}

TEST(PageMemoryRegionTest, LargePageMemoryRegion) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<LargePageMemoryRegion>(&allocator, 1024);
  pmr->UnprotectForTesting();
  // Only one PageMemory.
  const auto* pm = pmr->page_memory();
  EXPECT_LE(1024u, pm->writeable_region().size());
  EXPECT_EQ(0u, pm->writeable_region().base()[0]);
  EXPECT_EQ(0u, pm->writeable_region().end()[-1]);
}

TEST(PageMemoryRegionTest, PlatformUsesGuardPages) {
  // This tests that the testing allocator actually uses protected guard
  // regions.
  gc::test::BaseAllocator allocator;
#ifdef ARCH_CPU_PPC64
  EXPECT_FALSE(SupportsCommittingGuardPages(&allocator));
#else   // ARCH_CPU_PPC64
  EXPECT_TRUE(SupportsCommittingGuardPages(&allocator));
#endif  // ARCH_CPU_PPC64
}

namespace {

void access(volatile uint8_t) {}

}  // namespace

TEST(PageMemoryRegionDeathTest, ReservationIsFreed) {
  gc::test::BaseAllocator allocator;
  Address base;
  {
    auto pmr = std::make_unique<LargePageMemoryRegion>(&allocator, 1024);
    base = pmr->reserved_region().base();
  }
  EXPECT_DEATH(access(base[0]), "");
}

TEST(PageMemoryRegionDeathTest, FrontGuardPageAccessCrashes) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  if (SupportsCommittingGuardPages(&allocator)) {
    EXPECT_DEATH(access(pmr->begin()->overall_region().base()[0]), "");
  }
}

TEST(PageMemoryRegionDeathTest, BackGuardPageAccessCrashes) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  if (SupportsCommittingGuardPages(&allocator)) {
    EXPECT_DEATH(access(pmr->begin()->writeable_region().end()[0]), "");
  }
}

}  // namespace test
}  // namespace internal
}  // namespace gc
