// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/search_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

TEST(SearchUtilsTest, PrefixMatch) {
  // Query is a prefix of text, score is the ratio.
  EXPECT_NEAR(ExactPrefixMatchScore(base::UTF8ToUTF16("musi"),
                                    base::UTF8ToUTF16("music")),
              0.8, 0.001);

  // Text is a prefix of query, score is 0.
  EXPECT_EQ(ExactPrefixMatchScore(base::UTF8ToUTF16("music"),
                                  base::UTF8ToUTF16("musi")),
            0);

  // Case matters.
  EXPECT_EQ(ExactPrefixMatchScore(base::UTF8ToUTF16("musi"),
                                  base::UTF8ToUTF16("Music")),
            0);

  // Query isn't a prefix.
  EXPECT_EQ(ExactPrefixMatchScore(base::UTF8ToUTF16("wide"),
                                  base::UTF8ToUTF16("wifi")),
            0);

  // Text is empty.
  EXPECT_EQ(
      ExactPrefixMatchScore(base::UTF8ToUTF16("music"), base::UTF8ToUTF16("")),
      0);

  // Query is empty.
  EXPECT_EQ(
      ExactPrefixMatchScore(base::UTF8ToUTF16(""), base::UTF8ToUTF16("abc")),
      0);
}

TEST(SearchUtilsTest, BlockMatch) {
  EXPECT_NEAR(
      BlockMatchScore(base::UTF8ToUTF16("wifi"), base::UTF8ToUTF16("wi-fi")),
      0.804, 0.001);

  // Case matters.
  EXPECT_NEAR(
      BlockMatchScore(base::UTF8ToUTF16("wifi"), base::UTF8ToUTF16("Wi-Fi")),
      0.402, 0.001);
}

TEST(SearchUtilsTest, IsRelevantApproximately) {
  // Relevant because prefix requirement is met.
  EXPECT_TRUE(IsRelevantApproximately(
      base::UTF8ToUTF16("wifi"), base::UTF8ToUTF16("wi-fi"),
      0 /* prefix_threshold */, 0.99 /* block_threshold */));

  // Relevant because prefix requirement is met.
  EXPECT_TRUE(IsRelevantApproximately(
      base::UTF8ToUTF16("musi"), base::UTF8ToUTF16("music"),
      0.8 /* prefix_threshold */, 0.999 /* block_threshold */));

  // Relevant because block match requirement is met.
  EXPECT_TRUE(IsRelevantApproximately(
      base::UTF8ToUTF16("wifi"), base::UTF8ToUTF16("wi-fi"),
      0.2 /* prefix_threshold */, 0.001 /* block_threshold */));

  // Neither prefix nor block match requirement is met.
  EXPECT_FALSE(IsRelevantApproximately(
      base::UTF8ToUTF16("wifi"), base::UTF8ToUTF16("wi-fi"),
      0.2 /* prefix_threshold */, 0.99 /* block_threshold */));
}

}  // namespace local_search_service
}  // namespace chromeos
