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

TEST(NewLinkedHashSetTest, Insert) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.insert(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.insert(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  result = set.insert(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_TRUE(it == set.end());
}

TEST(NewLinkedHashSetTest, AppendOrMoveToLast) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.AppendOrMoveToLast(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.AppendOrMoveToLast(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.AppendOrMoveToLast(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 3);
}

TEST(NewLinkedHashSetTest, PrependOrMoveToFirst) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.PrependOrMoveToFirst(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.PrependOrMoveToFirst(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.PrependOrMoveToFirst(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

}  // namespace WTF
