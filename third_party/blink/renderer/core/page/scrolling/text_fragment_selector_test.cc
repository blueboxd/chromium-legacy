// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

#include <gtest/gtest.h>

namespace blink {

class TextFragmentSelectorTest : public testing::Test {
 protected:
  bool Equals(TextFragmentSelector a, TextFragmentSelector b) {
    return a.Type() == b.Type() && a.Start() == b.Start() &&
           a.End() == b.End() && a.Prefix() == b.Prefix() &&
           a.Suffix() == b.Suffix();
  }
};

TEST_F(TextFragmentSelectorTest, ExactText) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "", "",
                                "");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, ExactTextWithPrefix) {
  TextFragmentSelector selector = TextFragmentSelector::Create("prefix-,test");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "",
                                "prefix", "");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, ExactTextWithSuffix) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "", "",
                                "suffix");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, ExactTextWithContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "",
                                "prefix", "suffix");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, TextRange) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test,page");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "", "");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, TextRangeWithPrefix) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,page");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "prefix", "");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, TextRangeWithSuffix) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("test,page,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "", "suffix");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, TextRangeWithContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,page,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "prefix", "suffix");
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, InvalidContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix,test,page,suffix");
  TextFragmentSelector expected(TextFragmentSelector::kInvalid);
  EXPECT_TRUE(Equals(selector, expected));
}

TEST_F(TextFragmentSelectorTest, TooManyParameters) {
  TextFragmentSelector selector = TextFragmentSelector::Create(
      "prefix-,exact text, that has commas, which are not percent "
      "encoded,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kInvalid);
  EXPECT_TRUE(Equals(selector, expected));
}

}  // namespace blink
