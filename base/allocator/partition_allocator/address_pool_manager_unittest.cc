// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager.h"

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

TEST(AddressPoolManager, TooLargePool) {
  uintptr_t base_addr = 0x4200000;

  constexpr size_t kSize = 16ull * 1024 * 1024 * 1024;
  AddressPoolManager::GetInstance()->ResetForTesting();
  AddressPoolManager::GetInstance()->Add(base_addr, kSize);
  EXPECT_DEATH_IF_SUPPORTED(
      AddressPoolManager::GetInstance()->Add(base_addr, kSize + kSuperPageSize),
      "");
}

TEST(AddressPoolManager, OnePage) {
  uintptr_t base_addr = 0x4200000;
  char* base_ptr = reinterpret_cast<char*>(base_addr);

  AddressPoolManager::GetInstance()->ResetForTesting();
  pool_handle pool =
      AddressPoolManager::GetInstance()->Add(base_addr, kSuperPageSize);

  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize + 1),
            nullptr);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            base_ptr);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            nullptr);
  AddressPoolManager::GetInstance()->Free(pool, base_ptr, kSuperPageSize);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            base_ptr);
}

TEST(AddressPoolManager, ManyPages) {
  uintptr_t base_addr = 0x4200000;
  char* base_ptr = reinterpret_cast<char*>(base_addr);

  AddressPoolManager::GetInstance()->ResetForTesting();
  constexpr size_t kPageCnt = 8192;
  pool_handle pool = AddressPoolManager::GetInstance()->Add(
      base_addr, kPageCnt * kSuperPageSize);

  EXPECT_EQ(
      AddressPoolManager::GetInstance()->Alloc(pool, kPageCnt * kSuperPageSize),
      base_ptr);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            nullptr);
  AddressPoolManager::GetInstance()->Free(pool, base_ptr,
                                          kPageCnt * kSuperPageSize);
  EXPECT_EQ(
      AddressPoolManager::GetInstance()->Alloc(pool, kPageCnt * kSuperPageSize),
      base_ptr);
}

TEST(AddressPoolManager, PagesFragmented) {
  uintptr_t base_addr = 0x4200000;
  char* base_ptr = reinterpret_cast<char*>(base_addr);

  AddressPoolManager::GetInstance()->ResetForTesting();
  constexpr size_t kPageCnt = 8192;
  pool_handle pool = AddressPoolManager::GetInstance()->Add(
      base_addr, kPageCnt * kSuperPageSize);

  void* addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    AddressPoolManager::GetInstance()->Free(pool, addrs[i], kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, 2 * kSuperPageSize),
            nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize),
            nullptr);
}

TEST(AddressPoolManager, IrregularPattern) {
  uintptr_t base_addr = 0x4200000;
  char* base_ptr = reinterpret_cast<char*>(base_addr);

  AddressPoolManager::GetInstance()->ResetForTesting();
  constexpr size_t kPageCnt = 8192;
  pool_handle pool = AddressPoolManager::GetInstance()->Add(
      base_addr, kPageCnt * kSuperPageSize);

  void* a1 = AddressPoolManager::GetInstance()->Alloc(pool, kSuperPageSize);
  EXPECT_EQ(a1, base_ptr);
  void* a2 = AddressPoolManager::GetInstance()->Alloc(pool, 2 * kSuperPageSize);
  EXPECT_EQ(a2, base_ptr + 1 * kSuperPageSize);
  void* a3 = AddressPoolManager::GetInstance()->Alloc(pool, 3 * kSuperPageSize);
  EXPECT_EQ(a3, base_ptr + 3 * kSuperPageSize);
  void* a4 = AddressPoolManager::GetInstance()->Alloc(pool, 4 * kSuperPageSize);
  EXPECT_EQ(a4, base_ptr + 6 * kSuperPageSize);
  void* a5 = AddressPoolManager::GetInstance()->Alloc(pool, 5 * kSuperPageSize);
  EXPECT_EQ(a5, base_ptr + 10 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool, a4, 4 * kSuperPageSize);
  void* a6 = AddressPoolManager::GetInstance()->Alloc(pool, 6 * kSuperPageSize);
  EXPECT_EQ(a6, base_ptr + 15 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool, a5, 5 * kSuperPageSize);
  void* a7 = AddressPoolManager::GetInstance()->Alloc(pool, 7 * kSuperPageSize);
  EXPECT_EQ(a7, base_ptr + 6 * kSuperPageSize);
  void* a8 = AddressPoolManager::GetInstance()->Alloc(pool, 3 * kSuperPageSize);
  EXPECT_EQ(a8, base_ptr + 21 * kSuperPageSize);
  void* a9 = AddressPoolManager::GetInstance()->Alloc(pool, 2 * kSuperPageSize);
  EXPECT_EQ(a9, base_ptr + 13 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool, a7, 7 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool, a9, 2 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool, a6, 6 * kSuperPageSize);
  void* a10 =
      AddressPoolManager::GetInstance()->Alloc(pool, 15 * kSuperPageSize);
  EXPECT_EQ(a10, base_ptr + 6 * kSuperPageSize);
}

#endif  // defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

}  // namespace internal
}  // namespace base
