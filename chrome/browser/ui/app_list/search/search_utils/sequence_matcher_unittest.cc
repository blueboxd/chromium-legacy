// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class SequenceMatcherTest : public testing::Test {};

TEST_F(SequenceMatcherTest, TestEditDistance) {
  // Transposition
  ASSERT_EQ(SequenceMatcher("abcd", "abdc").EditDistance(), 1);

  // Deletion
  ASSERT_EQ(SequenceMatcher("abcde", "abcd").EditDistance(), 1);
  ASSERT_EQ(SequenceMatcher("12", "").EditDistance(), 2);

  // Insertion
  ASSERT_EQ(SequenceMatcher("abc", "abxbc").EditDistance(), 2);
  ASSERT_EQ(SequenceMatcher("", "abxbc").EditDistance(), 5);

  // Substitution
  ASSERT_EQ(SequenceMatcher("book", "back").EditDistance(), 2);

  // Combination
  ASSERT_EQ(SequenceMatcher("caclulation", "calculator").EditDistance(), 3);
  ASSERT_EQ(SequenceMatcher("sunday", "saturday").EditDistance(), 3);
}

}  // namespace app_list
