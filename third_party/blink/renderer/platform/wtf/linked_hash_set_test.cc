// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TEST(NewLinkedHashSetTest, Node) {
  NewLinkedHashSetNode<int> node0;
  NewLinkedHashSetNode<int> node1(1, 2, 3);

  EXPECT_EQ(node0.GetPrevIndexForUsedNode(), 0u);
  EXPECT_EQ(node0.GetNextIndexForUsedNode(), 0u);

  EXPECT_EQ(node1.GetPrevIndexForUsedNode(), 1u);
  EXPECT_EQ(node1.GetNextIndexForUsedNode(), 2u);
  node1.SetPrevIndexForUsedNode(3);
  EXPECT_EQ(node1.GetPrevIndexForUsedNode(), 3u);
  node1.SetNextIndexForUsedNode(4);
  EXPECT_EQ(node1.GetNextIndexForUsedNode(), 4u);
  EXPECT_EQ(node1.GetValueForUsedNode(), 3);
  node1.SetValueForUsedNode(-1);
  EXPECT_EQ(node1.GetValueForUsedNode(), -1);
}

TEST(NewLinkedHashSetTest, Construct) {
  NewLinkedHashSet<int> test;
  EXPECT_EQ(test.size(), 0u);
}

TEST(NewLinkedHashSetTest, Iterator) {
  using Set = NewLinkedHashSet<int>;

  Set set;
  EXPECT_TRUE(set.begin() == set.end());

  set.insert(1);
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_TRUE(it == set.end());

  set.insert(2);
  set.insert(3);

  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_TRUE(it == set.end());
  --it;
  EXPECT_EQ(*it, 3);
  --it;
  EXPECT_EQ(*it, 2);
  --it;
  EXPECT_EQ(*it, 1);
  EXPECT_TRUE(it == set.begin());
  EXPECT_TRUE(it != set.end());

  int i = 1;
  for (auto it : set) {
    EXPECT_EQ(it, i);
    i++;
  }
}

TEST(NewLinkedHashSetTest, InsertAndEraseForInteger) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.insert(1);  // set: 1 vector: anchor 1
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 1u);

  result = set.insert(2);  // set: 1, 2 vector: anchor 1, 2
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);
  EXPECT_EQ(set.size(), 2u);

  result = set.insert(1);  // set: 1, 2 vector: anchor 1, 2
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);
  EXPECT_EQ(set.size(), 2u);

  result = set.insert(3);  // set: 1, 2, 3 vector: anchor 1, 2, 3
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 3);
  EXPECT_EQ(set.size(), 3u);

  set.erase(1);  // set: 2, 3 vector: anchor hole 2, 3
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 3);
  EXPECT_EQ(set.size(), 2u);

  result = set.insert(1);  // set: 2, 3, 1 vector: anchor 1, 2, 3
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 3u);

  set.erase(3);  // set: 2, 1 vector: anchor 1, 2, hole
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 2u);

  result = set.insert(4);  // set: 2, 1, 4 vector: anchor 1, 2, 4
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 4);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 4);
  EXPECT_EQ(set.size(), 3u);

  result = set.insert(5);  // set: 2, 1, 4, 5 vector: anchor 1, 2, 4, 5
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 5);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 5);
  EXPECT_EQ(set.size(), 4u);

  set.erase(10);
  set.erase(100);
  set.erase(1000);
  set.erase(211);

  result = set.insert(3);  // set: 2, 1, 4, 5, 3 vector: anchor 1, 2, 4, 5, 3
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 3);
  EXPECT_EQ(set.size(), 5u);
}

TEST(NewLinkedHashSetTest, RemoveFirstAndPopBackForInteger) {
  using Set = NewLinkedHashSet<int>;
  Set set;

  for (int i = 1; i <= 5; i++) {
    set.insert(i);
  }
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 5);

  set.RemoveFirst();
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 5);

  set.pop_back();
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 4);

  set.RemoveFirst();
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 4);

  set.pop_back();
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 3);

  set.RemoveFirst();
  EXPECT_TRUE(set.IsEmpty());

  set.insert(1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);
  set.pop_back();
  EXPECT_TRUE(set.IsEmpty());
}

TEST(NewLinkedHashSetTest, PrependOrMoveToFirstForInteger) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result =
      set.PrependOrMoveToFirst(1);  // set: 1 vector: anchor 1
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 1u);

  result = set.insert(2);  // set: 1, 2 vector: anchor 1, 2
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);
  EXPECT_EQ(set.size(), 2u);

  result = set.PrependOrMoveToFirst(2);  // set: 2, 1 vector: anchor 1, 2
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 2u);

  result = set.PrependOrMoveToFirst(3);  // set: 3, 2, 1 vector: anchor 1, 2, 3
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 3u);
}

TEST(NewLinkedHashSetTest, AppendOrMoveToLastForInteger) {
  using Set = NewLinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.AppendOrMoveToLast(1);  // set: 1 vector: anchor 1
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 1u);

  result = set.insert(2);  // set: 1, 2 vector: anchor 1, 2
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);
  EXPECT_EQ(set.size(), 2u);

  result = set.AppendOrMoveToLast(1);  // set: 2, 1 vector: anchor 1, 2
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 1);
  EXPECT_EQ(set.size(), 2u);

  result = set.AppendOrMoveToLast(3);  // set: 2, 1, 3 vector: anchor 1, 2, 3
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);
  EXPECT_EQ(set.front(), 2);
  EXPECT_EQ(set.back(), 3);
  EXPECT_EQ(set.size(), 3u);
}

TEST(NewLinkedHashSetTest, InsertAndEraseForString) {
  using Set = NewLinkedHashSet<String>;
  Set set;
  Set::AddResult result = set.insert("a");  // set: "a" vector: anchor "a"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "a");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 1u);

  result = set.insert("b");  // set: "a" "b" vector: anchor "a" "b"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "b");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  result = set.insert("");  // set: "a" "b" "" vector: anchor "a" "b" ""
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "");
  EXPECT_EQ(set.size(), 3u);

  result = set.insert(
      "abc");  // set: "a" "b" "" "abc" vector: anchor "a" "b" "" "abc"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "abc");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "abc");
  EXPECT_EQ(set.size(), 4u);

  set.erase("");  // set: "a" "b" "abc" vector: anchor "a" "b" hole "abc"
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "abc");
  EXPECT_EQ(set.size(), 3u);

  set.erase("abc");  // set: "a" "b" vector: anchor "a" "b" hole hole
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  set.erase("");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  set.erase("c");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  set.insert("c");  // set: "a" "b" "c" vector: anchor "a" "b" hole "c"
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "c");
  EXPECT_EQ(set.size(), 3u);
}

TEST(NewLinkedHashSetTest, RemoveFirstAndPopBackForString) {
  using Set = NewLinkedHashSet<String>;
  Set set;

  set.insert("a");
  set.insert("b");
  set.insert("c");
  set.insert("d");
  set.insert("e");

  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "e");

  set.RemoveFirst();
  EXPECT_EQ(set.front(), "b");
  EXPECT_EQ(set.back(), "e");

  set.pop_back();
  EXPECT_EQ(set.front(), "b");
  EXPECT_EQ(set.back(), "d");

  set.RemoveFirst();
  EXPECT_EQ(set.front(), "c");
  EXPECT_EQ(set.back(), "d");

  set.pop_back();
  EXPECT_EQ(set.front(), "c");
  EXPECT_EQ(set.back(), "c");

  set.RemoveFirst();
  EXPECT_TRUE(set.IsEmpty());
}

TEST(NewLinkedHashSetTest, PrependOrMoveToFirstForString) {
  using Set = NewLinkedHashSet<String>;
  Set set;
  Set::AddResult result =
      set.PrependOrMoveToFirst("a");  // set: "a" vector: anchor "a"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "a");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 1u);

  result = set.insert("b");  // set: "a", "b" vector: anchor "a", "b"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "b");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  result =
      set.PrependOrMoveToFirst("b");  // set: "b", "a" vector: anchor "a", "b"
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "b");
  EXPECT_EQ(set.front(), "b");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 2u);

  result = set.PrependOrMoveToFirst(
      "c");  // set: "c", "b", "a" vector: anchor "a", "b", "c"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "c");
  EXPECT_EQ(set.front(), "c");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 3u);
}

TEST(NewLinkedHashSetTest, AppendOrMoveToLastForString) {
  using Set = NewLinkedHashSet<String>;
  Set set;
  Set::AddResult result =
      set.AppendOrMoveToLast("a");  // set: "a" vector: anchor "a"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "a");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 1u);

  result = set.insert("b");  // set: "a", "b" vector: anchor "a", "b"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "b");
  EXPECT_EQ(set.front(), "a");
  EXPECT_EQ(set.back(), "b");
  EXPECT_EQ(set.size(), 2u);

  result =
      set.AppendOrMoveToLast("a");  // set: "b", "a" vector: anchor "a", "b"
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "a");
  EXPECT_EQ(set.front(), "b");
  EXPECT_EQ(set.back(), "a");
  EXPECT_EQ(set.size(), 2u);

  result = set.AppendOrMoveToLast(
      "c");  // set: "b", "a", "c" vector: anchor "a", "b", "c"
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, "c");
  EXPECT_EQ(set.front(), "b");
  EXPECT_EQ(set.back(), "c");
  EXPECT_EQ(set.size(), 3u);
}

}  // namespace WTF
