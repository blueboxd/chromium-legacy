// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TEST(NewLinkedHashSetTest, Construct) {
  NewLinkedHashSet<int> test;
}

TEST(NewLinkedHashSetTest, Iterator) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  EXPECT_TRUE(set.begin() == set.end());
  EXPECT_TRUE(set.rbegin() == set.rend());
}

}  // namespace WTF
