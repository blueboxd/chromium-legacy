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
  EXPECT_EQ(test.size(), 0u);
  EXPECT_TRUE(test.IsEmpty());
}

TEST(NewLinkedHashSetTest, Iterator) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  EXPECT_TRUE(set.begin() == set.end());
  EXPECT_TRUE(set.rbegin() == set.rend());
}

TEST(NewLinkedHashSetTest, FrontAndBack) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  EXPECT_EQ(set.size(), 0u);
  EXPECT_TRUE(set.IsEmpty());

  set.PrependOrMoveToFirst(1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);

  set.insert(2);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);

  set.AppendOrMoveToLast(3);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 3);

  set.PrependOrMoveToFirst(3);
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 2);

  set.AppendOrMoveToLast(1);
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 1);
}

TEST(NewLinkedHashSetTest, FindAndContains) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  set.insert(2);
  set.AppendOrMoveToLast(2);
  set.PrependOrMoveToFirst(1);
  set.insert(3);
  set.AppendOrMoveToLast(4);
  set.insert(5);

  int i = 1;
  for (auto element : set) {
    EXPECT_EQ(element, i);
    i++;
  }

  Set::const_iterator it = set.find(2);
  EXPECT_EQ(*it, 2);
  it = set.find(3);
  EXPECT_EQ(*it, 3);
  it = set.find(10);
  EXPECT_TRUE(it == set.end());

  EXPECT_TRUE(set.Contains(1));
  EXPECT_TRUE(set.Contains(2));
  EXPECT_TRUE(set.Contains(3));
  EXPECT_TRUE(set.Contains(4));
  EXPECT_TRUE(set.Contains(5));

  EXPECT_FALSE(set.Contains(10));
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

TEST(NewLinkedHashSetTest, Erase) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);
  set.insert(4);
  set.insert(5);

  Set::const_iterator it = set.begin();
  ++it;
  set.erase(it);
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);

  set.erase(3);
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);

  set.insert(6);
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);
  ++it;
  EXPECT_EQ(*it, 6);
}

TEST(NewLinkedHashSetTest, RemoveFirst) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.RemoveFirst();
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);

  set.RemoveFirst();
  it = set.begin();
  EXPECT_EQ(*it, 3);

  set.RemoveFirst();
  EXPECT_TRUE(set.begin() == set.end());
}

TEST(NewLinkedHashSetTest, pop_back) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.pop_back();
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  set.pop_back();
  it = set.begin();
  EXPECT_EQ(*it, 1);

  set.pop_back();
  EXPECT_TRUE(set.begin() == set.end());
}

TEST(NewLinkedHashSetTest, Clear) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.clear();
  EXPECT_TRUE(set.begin() == set.end());

  set.insert(1);
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_TRUE(it == set.end());
}

}  // namespace WTF
