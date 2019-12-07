// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_matching/sequence_matcher.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr bool kDefaultUseEditDistance = false;

using Match = SequenceMatcher::Match;
bool MatchEqual(const Match& match1, const Match& match2) {
  return match1.pos_first_string == match2.pos_first_string &&
         match1.pos_second_string == match2.pos_second_string &&
         match1.length == match2.length;
}

class SequenceMatcherTest : public testing::Test {};

TEST_F(SequenceMatcherTest, TestEditDistance) {
  // Transposition
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("abcd"),
                            base::UTF8ToUTF16("abdc"), kDefaultUseEditDistance)
                .EditDistance(),
            1);

  // Deletion
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("abcde"),
                            base::UTF8ToUTF16("abcd"), kDefaultUseEditDistance)
                .EditDistance(),
            1);
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("12"), base::UTF8ToUTF16(""),
                            kDefaultUseEditDistance)
                .EditDistance(),
            2);

  // Insertion
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("abc"),
                            base::UTF8ToUTF16("abxbc"), kDefaultUseEditDistance)
                .EditDistance(),
            2);
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16(""), base::UTF8ToUTF16("abxbc"),
                            kDefaultUseEditDistance)
                .EditDistance(),
            5);

  // Substitution
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("book"),
                            base::UTF8ToUTF16("back"), kDefaultUseEditDistance)
                .EditDistance(),
            2);

  // Combination
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("caclulation"),
                      base::UTF8ToUTF16("calculator"), kDefaultUseEditDistance)
          .EditDistance(),
      3);
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("sunday"),
                      base::UTF8ToUTF16("saturday"), kDefaultUseEditDistance)
          .EditDistance(),
      3);
}

TEST_F(SequenceMatcherTest, TestFindLongestMatch) {
  SequenceMatcher sequence_match(base::UTF8ToUTF16("miscellanious"),
                                 base::UTF8ToUTF16("miscellaneous"),
                                 kDefaultUseEditDistance);
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(0, 13, 0, 13),
                         Match(0, 0, 9)));
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(7, 13, 7, 13),
                         Match(10, 10, 3)));

  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(base::UTF8ToUTF16(""), base::UTF8ToUTF16("abcd"),
                      kDefaultUseEditDistance)
          .FindLongestMatch(0, 0, 0, 4),
      Match(0, 0, 0)));
  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(base::UTF8ToUTF16("abababbababa"),
                      base::UTF8ToUTF16("ababbaba"), kDefaultUseEditDistance)
          .FindLongestMatch(0, 12, 0, 8),
      Match(2, 0, 8)));
  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(base::UTF8ToUTF16("aaaaaa"), base::UTF8ToUTF16("aaaaa"),
                      kDefaultUseEditDistance)
          .FindLongestMatch(0, 6, 0, 5),
      Match(0, 0, 5)));
}

TEST_F(SequenceMatcherTest, TestGetMatchingBlocks) {
  SequenceMatcher sequence_match(
      base::UTF8ToUTF16("This is a demo sentence!!!"),
      base::UTF8ToUTF16("This demo sentence is good!!!"),
      kDefaultUseEditDistance);
  const std::vector<Match> true_matches = {Match(0, 0, 4), Match(9, 4, 14),
                                           Match(23, 26, 3), Match(26, 29, 0)};
  const std::vector<Match> matches = sequence_match.GetMatchingBlocks();
  ASSERT_EQ(matches.size(), 4ul);
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(MatchEqual(matches[i], true_matches[i]));
  }
}

TEST_F(SequenceMatcherTest, TestSequenceMatcherRatio) {
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("abcd"),
                            base::UTF8ToUTF16("adbc"), kDefaultUseEditDistance)
                .Ratio(),
            0.75);
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("white cats"),
                      base::UTF8ToUTF16("cats white"), kDefaultUseEditDistance)
          .Ratio(),
      0.5);
}

TEST_F(SequenceMatcherTest, TestEditDistanceRatio) {
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("abcd"),
                            base::UTF8ToUTF16("adbc"), true)
                .Ratio(),
            0.5);
  EXPECT_NEAR(SequenceMatcher(base::UTF8ToUTF16("white cats"),
                              base::UTF8ToUTF16("cats white"), true)
                  .Ratio(),
              0.2, 0.01);
}
}  // namespace
