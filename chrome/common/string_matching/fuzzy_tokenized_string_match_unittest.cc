// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_matching/fuzzy_tokenized_string_match.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/string_matching/sequence_matcher.h"
#include "chrome/common/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr double kPartialMatchPenaltyRate = 0.9;

class FuzzyTokenizedStringMatchTest : public testing::Test {};

// TODO(crbug.com/1018613): update the tests once params are consolidated.
TEST_F(FuzzyTokenizedStringMatchTest, PartialRatioTest) {
  FuzzyTokenizedStringMatch match;
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("abcde"),
                                 base::UTF8ToUTF16("ababcXXXbcdeY"),
                                 kPartialMatchPenaltyRate, false),
              0.6, 0.01);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("big string"),
                                 base::UTF8ToUTF16("strength"),
                                 kPartialMatchPenaltyRate, false),
              0.71, 0.01);
  EXPECT_EQ(match.PartialRatio(base::UTF8ToUTF16("abc"), base::UTF8ToUTF16(""),
                               kPartialMatchPenaltyRate, false),
            0);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("different in order"),
                                 base::UTF8ToUTF16("order text"),
                                 kPartialMatchPenaltyRate, false),
              0.67, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("order different in"));
    base::string16 text(base::UTF8ToUTF16("text order"));
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false),
        0.67, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text"));
    base::string16 text(
        base::UTF8ToUTF16("this text is really really really long"));
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false),
        0.57, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("common string"));
    base::string16 text(base::UTF8ToUTF16("nothing is shared"));
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text), true,
                            kPartialMatchPenaltyRate, false),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false),
        0.33, 0.01);
  }
  {
    base::string16 query(
        base::UTF8ToUTF16("token shared token same shared same"));
    base::string16 text(base::UTF8ToUTF16("token shared token text text long"));
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false),
        0.83, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSortRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("order different in"));
    base::string16 text(base::UTF8ToUTF16("text order"));
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false),
        0.67, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false),
        0.36, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text"));
    base::string16 text(
        base::UTF8ToUTF16("this text is really really really long"));
    EXPECT_EQ(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false),
        0.5 * std::pow(0.9, 1));
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false),
        0.33, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("common string"));
    base::string16 text(base::UTF8ToUTF16("nothing is shared"));
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false),
        0.33, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, WeightedRatio) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false),
        0.67, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false),
        0.81, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("final fantasy"));
    base::string16 text(base::UTF8ToUTF16("finalfantasy"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false),
        0.96, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text!!!"));
    base::string16 text(
        base::UTF8ToUTF16("this sentence is much much much much much longer "
                          "than the text before"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false),
        0.85, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, FirstCharacterMatchTest) {
  {
    base::string16 query(base::UTF8ToUTF16("COC"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::FirstCharacterMatch(
                  TokenizedString(query), TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::FirstCharacterMatch(
                  TokenizedString(query), TokenizedString(text)),
              0.8);
  }
  {
    base::string16 query(base::UTF8ToUTF16("C o C"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::FirstCharacterMatch(
                  TokenizedString(query), TokenizedString(text)),
              0.0);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, PrefixMatchTest) {
  {
    base::string16 query(base::UTF8ToUTF16("clas"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatch(TokenizedString(query),
                                                     TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("clash clan"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatch(TokenizedString(query),
                                                     TokenizedString(text)),
              0.9);
  }
  {
    base::string16 query(base::UTF8ToUTF16("c o c"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatch(TokenizedString(query),
                                                     TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("clam"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatch(TokenizedString(query),
                                                     TokenizedString(text)),
              0.0);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest1) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.4, false, true, false,
                                  kPartialMatchPenaltyRate));
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                 0.4, false, true, false,
                                 kPartialMatchPenaltyRate));
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                 0.4, false, true, false,
                                 kPartialMatchPenaltyRate));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest2) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate));
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                 0.5, false, true, false,
                                 kPartialMatchPenaltyRate));
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, OtherParamTest) {
  FuzzyTokenizedStringMatch match;
  base::string16 query(base::UTF8ToUTF16("anonymous"));
  base::string16 text(base::UTF8ToUTF16("famous"));
  EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                0.35, false, false, true,
                                kPartialMatchPenaltyRate));
  EXPECT_NEAR(match.relevance(), 0.33 / 2, 0.01);
}

}  // namespace
