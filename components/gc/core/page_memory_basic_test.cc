// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/core/page_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gc {
namespace internal {
namespace test {

TEST(MemoryRegionTest, Construct) {
  uint8_t dummy;
  constexpr size_t kSize = 17;
  const MemoryRegion region(&dummy, kSize);
  EXPECT_EQ(&dummy, region.base());
  EXPECT_EQ(kSize, region.size());
  EXPECT_EQ(&dummy + kSize, region.end());
}

TEST(MemoryRegionTest, ContainsAddress) {
  uint8_t dummy;
  constexpr size_t kSize = 7;
  const MemoryRegion region(&dummy, kSize);
  EXPECT_FALSE(region.Contains(&dummy - 1));
  EXPECT_TRUE(region.Contains(&dummy));
  EXPECT_TRUE(region.Contains(&dummy + kSize - 1));
  EXPECT_FALSE(region.Contains(&dummy + kSize));
}

TEST(MemoryRegionTest, ContainsMemoryRegion) {
  uint8_t dummy;
  constexpr size_t kSize = 7;
  const MemoryRegion region(&dummy, kSize);
  const MemoryRegion contained_region1(&dummy, kSize - 1);
  EXPECT_TRUE(region.Contains(contained_region1));
  const MemoryRegion contained_region2(&dummy + 1, kSize - 1);
  EXPECT_TRUE(region.Contains(contained_region2));
  const MemoryRegion not_contained_region1(&dummy - 1, kSize);
  EXPECT_FALSE(region.Contains(not_contained_region1));
  const MemoryRegion not_contained_region2(&dummy + kSize, 1);
  EXPECT_FALSE(region.Contains(not_contained_region2));
}

TEST(PageMemoryTest, Construct) {
  uint8_t dummy;
  constexpr size_t kOverallSize = 17;
  const MemoryRegion overall_region(&dummy, kOverallSize);
  const MemoryRegion writeable_region(&dummy + 1, kOverallSize - 2);
  const PageMemory page_memory(overall_region, writeable_region);
  EXPECT_EQ(&dummy, page_memory.overall_region().base());
  EXPECT_EQ(&dummy + kOverallSize, page_memory.overall_region().end());
  EXPECT_EQ(&dummy + 1, page_memory.writeable_region().base());
  EXPECT_EQ(&dummy + kOverallSize - 1, page_memory.writeable_region().end());
}

#if DCHECK_IS_ON()

TEST(PageMemoryDeathTest, ConstructNonContainedRegions) {
  uint8_t dummy;
  constexpr size_t kOverallSize = 17;
  const MemoryRegion overall_region(&dummy, kOverallSize);
  const MemoryRegion writeable_region(&dummy + 1, kOverallSize);
  EXPECT_DEATH_IF_SUPPORTED(PageMemory(overall_region, writeable_region), "");
}

#endif  // DCHECK_IS_ON()

}  // namespace test
}  // namespace internal
}  // namespace gc
