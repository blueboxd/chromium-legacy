// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "build/build_config.h"
#include "components/gc/core/page_memory.h"
#include "components/gc/test/base_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gc {
namespace internal {
namespace test {

TEST(PageMemoryRegionTest, NormalPageMemoryRegion) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  pmr->UnprotectForTesting();
  MemoryRegion prev_overall;
  for (size_t i = 0; i < NormalPageMemoryRegion::kNumPageRegions; ++i) {
    const PageMemory pm = pmr->GetPageMemory(i);
    // Previous PageMemory aligns with the current one.
    if (prev_overall.base()) {
      EXPECT_EQ(prev_overall.end(), pm.overall_region().base());
    }
    prev_overall =
        MemoryRegion(pm.overall_region().base(), pm.overall_region().size());
    // Writeable region is contained in overall region.
    EXPECT_TRUE(pm.overall_region().Contains(pm.writeable_region()));
    EXPECT_EQ(0u, pm.writeable_region().base()[0]);
    EXPECT_EQ(0u, pm.writeable_region().end()[-1]);
    // Front guard page.
    EXPECT_EQ(pm.writeable_region().base(),
              pm.overall_region().base() + kGuardPageSize);
    // Back guard page.
    EXPECT_EQ(pm.overall_region().end(),
              pm.writeable_region().end() + kGuardPageSize);
  }
}

TEST(PageMemoryRegionTest, LargePageMemoryRegion) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<LargePageMemoryRegion>(&allocator, 1024);
  pmr->UnprotectForTesting();
  const PageMemory pm = pmr->GetPageMemory();
  EXPECT_LE(1024u, pm.writeable_region().size());
  EXPECT_EQ(0u, pm.writeable_region().base()[0]);
  EXPECT_EQ(0u, pm.writeable_region().end()[-1]);
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
    EXPECT_DEATH(access(pmr->GetPageMemory(0).overall_region().base()[0]), "");
  }
}

TEST(PageMemoryRegionDeathTest, BackGuardPageAccessCrashes) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  if (SupportsCommittingGuardPages(&allocator)) {
    EXPECT_DEATH(access(pmr->GetPageMemory(0).writeable_region().end()[0]), "");
  }
}

TEST(PageMemoryRegionTreeTest, AddNormalLookupRemove) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  PageMemoryRegionTree tree;
  tree.Add(pmr.get());
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().end() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end()));
  tree.Remove(pmr.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end() - 1));
}

TEST(PageMemoryRegionTreeTest, AddLargeLookupRemove) {
  gc::test::BaseAllocator allocator;
  constexpr size_t kLargeSize = 5012;
  auto pmr = std::make_unique<LargePageMemoryRegion>(&allocator, kLargeSize);
  PageMemoryRegionTree tree;
  tree.Add(pmr.get());
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(pmr.get(), tree.Lookup(pmr->reserved_region().end() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base() - 1));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end()));
  tree.Remove(pmr.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr->reserved_region().end() - 1));
}

TEST(PageMemoryRegionTreeTest, AddLookupRemoveMultiple) {
  gc::test::BaseAllocator allocator;
  auto pmr1 = std::make_unique<NormalPageMemoryRegion>(&allocator);
  constexpr size_t kLargeSize = 3127;
  auto pmr2 = std::make_unique<LargePageMemoryRegion>(&allocator, kLargeSize);
  PageMemoryRegionTree tree;
  tree.Add(pmr1.get());
  tree.Add(pmr2.get());
  ASSERT_EQ(pmr1.get(), tree.Lookup(pmr1->reserved_region().base()));
  ASSERT_EQ(pmr1.get(), tree.Lookup(pmr1->reserved_region().end() - 1));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().end() - 1));
  tree.Remove(pmr1.get());
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(pmr2.get(), tree.Lookup(pmr2->reserved_region().end() - 1));
  tree.Remove(pmr2.get());
  ASSERT_EQ(nullptr, tree.Lookup(pmr2->reserved_region().base()));
  ASSERT_EQ(nullptr, tree.Lookup(pmr2->reserved_region().end() - 1));
}

TEST(NormalPageMemoryPool, ConstructorEmpty) {
  gc::test::BaseAllocator allocator;
  NormalPageMemoryPool pool;
  EXPECT_EQ(nullptr, pool.Take());
}

TEST(NormalPageMemoryPool, AddTakeSameBucket) {
  gc::test::BaseAllocator allocator;
  auto pmr = std::make_unique<NormalPageMemoryRegion>(&allocator);
  const PageMemory pm = pmr->GetPageMemory(0);
  NormalPageMemoryPool pool;
  pool.Add(pm.writeable_region().base());
  EXPECT_EQ(pm.writeable_region().base(), pool.Take());
}

TEST(PageBackendTest, AllocateNormalUsesPool) {
  gc::test::BaseAllocator allocator;
  PageBackend backend(&allocator);
  Address writeable_base1 = backend.AllocateNormalPageMemory();
  EXPECT_NE(nullptr, writeable_base1);
  backend.FreeNormalPageMemory(writeable_base1);
  Address writeable_base2 = backend.AllocateNormalPageMemory();
  EXPECT_NE(nullptr, writeable_base2);
  EXPECT_EQ(writeable_base1, writeable_base2);
}

TEST(PageBackendTest, AllocateLarge) {
  gc::test::BaseAllocator allocator;
  PageBackend backend(&allocator);
  Address writeable_base1 = backend.AllocateLargePageMemory(13731);
  EXPECT_NE(nullptr, writeable_base1);
  Address writeable_base2 = backend.AllocateLargePageMemory(9478);
  EXPECT_NE(nullptr, writeable_base2);
  EXPECT_NE(writeable_base1, writeable_base2);
  backend.FreeLargePageMemory(writeable_base1);
  backend.FreeLargePageMemory(writeable_base2);
}

TEST(PageBackendTest, LookupNormal) {
  gc::test::BaseAllocator allocator;
  PageBackend backend(&allocator);
  Address writeable_base = backend.AllocateNormalPageMemory();
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - kGuardPageSize));
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - 1));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base + kPageSize -
                                           2 * kGuardPageSize - 1));
  EXPECT_EQ(nullptr,
            backend.Lookup(writeable_base + kPageSize - 2 * kGuardPageSize));
  EXPECT_EQ(nullptr,
            backend.Lookup(writeable_base - kGuardPageSize + kPageSize - 1));
}

TEST(PageBackendTest, LookupLarge) {
  gc::test::BaseAllocator allocator;
  PageBackend backend(&allocator);
  constexpr size_t kSize = 7934;
  Address writeable_base = backend.AllocateLargePageMemory(kSize);
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - kGuardPageSize));
  EXPECT_EQ(nullptr, backend.Lookup(writeable_base - 1));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base));
  EXPECT_EQ(writeable_base, backend.Lookup(writeable_base + kSize - 1));
}

TEST(PageBackendDeathTest, DestructingBackendDestroysPageMemory) {
  gc::test::BaseAllocator allocator;
  Address base;
  {
    PageBackend backend(&allocator);
    base = backend.AllocateNormalPageMemory();
  }
  EXPECT_DEATH(access(base[0]), "");
}

}  // namespace test
}  // namespace internal
}  // namespace gc
