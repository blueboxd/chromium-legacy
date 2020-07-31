// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commander {
TEST(CommanderFuzzyFinder, NonmatchIsZero) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("orangutan"), &ranges));
  EXPECT_TRUE(ranges.empty());
  EXPECT_EQ(0, FuzzyFind(base::ASCIIToUTF16("elephant"),
                         base::ASCIIToUTF16("orangutan"), &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, ExactMatchIsOne) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("orange"), &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

TEST(CommanderFuzzyFinder, CaseInsensitive) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(base::ASCIIToUTF16("orange"),
                         base::ASCIIToUTF16("Orange"), &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

TEST(CommanderFuzzyFinder, PrefixRanksHigherThanInternal) {
  std::vector<gfx::Range> ranges;

  double prefix_rank = FuzzyFind(base::ASCIIToUTF16("orange"),
                                 base::ASCIIToUTF16("Orange juice"), &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));

  double non_prefix_rank =
      FuzzyFind(base::ASCIIToUTF16("orange"),
                base::ASCIIToUTF16("William of Orange"), &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{11, 6}}));

  EXPECT_GT(prefix_rank, 0);
  EXPECT_GT(non_prefix_rank, 0);
  EXPECT_LT(prefix_rank, 1);
  EXPECT_LT(non_prefix_rank, 1);
  EXPECT_GT(prefix_rank, non_prefix_rank);
}
}  // namespace commander
