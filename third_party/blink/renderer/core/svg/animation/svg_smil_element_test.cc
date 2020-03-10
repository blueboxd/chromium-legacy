// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

Vector<std::pair<SMILTime, SMILTimeOrigin>> ExtractListContents(
    const SMILInstanceTimeList& list) {
  Vector<std::pair<SMILTime, SMILTimeOrigin>> times;
  for (const auto& item : list)
    times.push_back(std::make_pair(item.Time(), item.Origin()));
  return times;
}

TEST(SMILInstanceTimeListTest, Sort) {
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);
  list.Sort();

  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute}});
  ASSERT_EQ(ExtractListContents(list), expected_times);
}

TEST(SMILInstanceTimeListTest, InsertSortedAndUnique) {
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 3u);

  // Unique time/item.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(4),
                             SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 4u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times1(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript}});
  ASSERT_EQ(ExtractListContents(list), expected_times1);

  // Non-unique item.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(2),
                             SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 4u);
  ASSERT_EQ(ExtractListContents(list), expected_times1);

  // Same time but different origin.
  list.InsertSortedAndUnique(SMILTime::FromSecondsD(2),
                             SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times2(
      {{SMILTime::FromSecondsD(1), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(2), SMILTimeOrigin::kScript},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript}});
  ASSERT_EQ(ExtractListContents(list), expected_times2);
}

TEST(SMILInstanceTimeListTest, RemoveWithOrigin) {
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);

  list.RemoveWithOrigin(SMILTimeOrigin::kScript);
  ASSERT_EQ(list.size(), 3u);
  Vector<std::pair<SMILTime, SMILTimeOrigin>> expected_times(
      {{SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute},
       {SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute}});
  ASSERT_EQ(ExtractListContents(list), expected_times);
}

TEST(SMILInstanceTimeListTest, NextAfter) {
  SMILInstanceTimeList list;
  list.Append(SMILTime::FromSecondsD(1), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(2), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(3), SMILTimeOrigin::kAttribute);
  list.Append(SMILTime::FromSecondsD(4), SMILTimeOrigin::kScript);
  list.Append(SMILTime::FromSecondsD(5), SMILTimeOrigin::kAttribute);
  ASSERT_EQ(list.size(), 5u);

  // Just before an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2) - SMILTime::Epsilon()),
            SMILTime::FromSecondsD(2));
  // Equal to an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2)),
            SMILTime::FromSecondsD(3));
  // Just after an entry in the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(2) + SMILTime::Epsilon()),
            SMILTime::FromSecondsD(3));
  // Equal to the last entry in the the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(5)), SMILTime::Unresolved());
  // After the last entry in the the list.
  EXPECT_EQ(list.NextAfter(SMILTime::FromSecondsD(6)), SMILTime::Unresolved());
}

}  // namespace

}  // namespace blink
