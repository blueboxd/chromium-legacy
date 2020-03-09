// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/vector_backed_linked_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(VectorBackedLinkedListTest, Insert) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  EXPECT_TRUE(list.begin() == list.end());
  list.insert(list.end(), 1);
  list.insert(list.begin(), -2);
  list.insert(list.end(), 2);

  List::iterator it = list.begin();
  ++it;
  list.insert(it, -1);
  list.insert(it, 0);

  EXPECT_EQ(list.front(), -2);
  EXPECT_EQ(list.back(), 2);
  EXPECT_EQ(list.size(), 5u);

  int i = -2;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedList, PushFront) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  list.push_front(3);
  EXPECT_EQ(list.front(), 3);
  list.push_front(2);
  EXPECT_EQ(list.front(), 2);
  list.push_front(1);
  EXPECT_EQ(list.front(), 1);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedList, PushBack) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  list.push_back(1);
  EXPECT_EQ(list.back(), 1);
  list.push_back(2);
  EXPECT_EQ(list.back(), 2);
  list.push_back(3);
  EXPECT_EQ(list.back(), 3);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedList, MoveTo) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  List::iterator target = list.begin();
  list.MoveTo(target, list.end());  // {2, 3, 1}

  List::iterator it = list.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  --it;

  target = it;
  list.MoveTo(target, list.begin());  // {3, 2, 1}
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 1);

  target = it;
  list.MoveTo(target, --it);  // {3, 1, 2}
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

TEST(VectorBackedLinkedList, Iterator) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  List::iterator it = list.begin();

  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  *it = 4;  // list: {1, 2, 4}
  EXPECT_EQ(list.back(), 4);
  ++it;
  EXPECT_TRUE(it == list.end());
  --it;
  --it;
  --it;
  EXPECT_TRUE(it == list.begin());
  EXPECT_EQ(list.front(), 1);
  *it = 0;
  EXPECT_EQ(list.front(), 0);  // list: {0, 2, 4}

  List::reverse_iterator rit = list.rbegin();

  EXPECT_EQ(*rit, 4);
  ++rit;
  EXPECT_EQ(*rit, 2);
  ++rit;
  EXPECT_EQ(*rit, 0);
  EXPECT_FALSE(rit == list.rend());
  *rit = 1;  // list: {1, 2, 4}
  EXPECT_EQ(list.front(), 1);
  ++rit;
  EXPECT_TRUE(rit == list.rend());
  --rit;
  EXPECT_EQ(*rit, 1);
}

TEST(VectorBackedLinkedList, ConstIterator) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  List::const_iterator cit = list.cbegin();

  EXPECT_EQ(*cit, 1);
  ++cit;
  EXPECT_EQ(*cit, 2);
  ++cit;
  EXPECT_EQ(*cit, 3);
  ++cit;
  EXPECT_TRUE(cit == list.cend());
  --cit;
  --cit;
  --cit;
  EXPECT_TRUE(cit == list.cbegin());
  EXPECT_EQ(list.front(), 1);

  List::const_reverse_iterator crit = list.crbegin();

  EXPECT_EQ(*crit, 3);
  ++crit;
  EXPECT_EQ(*crit, 2);
  ++crit;
  EXPECT_EQ(*crit, 1);
  ++crit;
  EXPECT_TRUE(crit == list.crend());
  --crit;
  EXPECT_EQ(*crit, 1);
}

}  // namespace WTF
