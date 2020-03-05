// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gc/test/base_allocator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gc {
namespace test {

TEST(BaseAllocatorTest, AllocateAndFreePage) {
  BaseAllocator alloc;

  const size_t allocation_size = alloc.AllocatePageSize();
  uint8_t* memory = static_cast<uint8_t*>(
      alloc.AllocatePages(nullptr, allocation_size, alloc.AllocatePageSize(),
                          BaseAllocator::Permission::kReadWrite));
  ASSERT_TRUE(memory);
  memory[0] = 1;
  ASSERT_EQ(1u, memory[0]);
  memory[allocation_size - 1] = 2;
  ASSERT_EQ(2u, memory[allocation_size - 1]);
  ASSERT_TRUE(alloc.FreePages(memory, allocation_size));
}

}  // namespace test
}  // namespace gc
