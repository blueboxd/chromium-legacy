// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class AtomicMemcpyTest : public ::testing::Test {};

template <size_t buffer_size>
void TestAtomicMemcpy() {
  unsigned char src[buffer_size];
  for (size_t i = 0; i < buffer_size; ++i)
    src[i] = static_cast<char>(i + 1);
  // Allocating extra memory before and after the buffer to make sure the
  // atomic memcpy doesn't exceed the buffer in any direction.
  unsigned char tgt[buffer_size + (2 * sizeof(size_t))];
  memset(tgt, 0, buffer_size + (2 * sizeof(size_t)));
  AtomicMemcpy<buffer_size>(tgt + sizeof(size_t), src);
  // Check nothing before the buffer was changed
  EXPECT_EQ(0u, *reinterpret_cast<size_t*>(&tgt[0]));
  // Check buffer was copied correctly
  EXPECT_TRUE(!memcmp(src, tgt + sizeof(size_t), buffer_size));
  // Check nothing after the buffer was changed
  EXPECT_EQ(0u, *reinterpret_cast<size_t*>(&tgt[sizeof(size_t) + buffer_size]));
}

TEST_F(AtomicMemcpyTest, UINT8T) {
  TestAtomicMemcpy<sizeof(uint8_t)>();
}
TEST_F(AtomicMemcpyTest, UINT16T) {
  TestAtomicMemcpy<sizeof(uint16_t)>();
}
TEST_F(AtomicMemcpyTest, UINT32T) {
  TestAtomicMemcpy<sizeof(uint32_t)>();
}
TEST_F(AtomicMemcpyTest, UINT64T) {
  TestAtomicMemcpy<sizeof(uint64_t)>();
}

// Tests for sizes that don't match a specific promitive type:
TEST_F(AtomicMemcpyTest, 17Bytes) {
  TestAtomicMemcpy<17>();
}
TEST_F(AtomicMemcpyTest, 34Bytes) {
  TestAtomicMemcpy<34>();
}
TEST_F(AtomicMemcpyTest, 68Bytes) {
  TestAtomicMemcpy<68>();
}
TEST_F(AtomicMemcpyTest, 127Bytes) {
  TestAtomicMemcpy<127>();
}

}  // namespace WTF
