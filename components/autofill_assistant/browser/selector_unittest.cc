// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

TEST(SelectorTest, Constructor_Simple) {
  Selector selector({"#test"});
  ASSERT_EQ(1, selector.proto.filters().size());
  EXPECT_EQ("#test", selector.proto.filters(0).css_selector());
}

TEST(SelectorTest, Constructor_WithIframe) {
  Selector selector({"#frame", "#test"});
  ASSERT_EQ(4, selector.proto.filters().size());
  EXPECT_EQ("#frame", selector.proto.filters(0).css_selector());
  EXPECT_EQ(selector.proto.filters(1).filter_case(),
            SelectorProto::Filter::kPickOne);
  EXPECT_EQ(selector.proto.filters(2).filter_case(),
            SelectorProto::Filter::kEnterFrame);
  EXPECT_EQ("#test", selector.proto.filters(3).css_selector());
}

TEST(SelectorTest, FromProto) {
  SelectorProto proto;
  proto.add_filters()->set_css_selector("#test");

  EXPECT_EQ(Selector({"#test"}), Selector(proto));
}

TEST(SelectorTest, Comparison) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_LT(Selector({"a"}), Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));
}

TEST(SelectorTest, Comparison_PseudoType) {
  EXPECT_FALSE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
               Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_LT(Selector({"a"}).SetPseudoType(PseudoType::BEFORE),
            Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_LT(Selector({"b"}), Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
  EXPECT_TRUE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
              Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
}

TEST(SelectorTest, Comparison_Visibility) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"a"}).MustBeVisible());
  EXPECT_LT(Selector({"a"}), Selector({"a"}).MustBeVisible());
  EXPECT_TRUE(Selector({"a"}).MustBeVisible() ==
              Selector({"a"}).MustBeVisible());
}

TEST(SelectorTest, Comparison_InnerText) {
  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a") ==
               Selector({"a"}).MatchingInnerText("b"));
  EXPECT_LT(Selector({"a"}).MatchingInnerText("a"),
            Selector({"a"}).MatchingInnerText("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a") ==
              Selector({"a"}).MatchingInnerText("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a", false) ==
               Selector({"a"}).MatchingInnerText("a", true));
  EXPECT_LT(Selector({"a"}).MatchingInnerText("a", false),
            Selector({"a"}).MatchingInnerText("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a", true) ==
              Selector({"a"}).MatchingInnerText("a", true));
}

TEST(SelectorTest, Comparison_Value) {
  EXPECT_FALSE(Selector({"a"}).MatchingValue("a") ==
               Selector({"a"}).MatchingValue("b"));
  EXPECT_LT(Selector({"a"}).MatchingValue("a"),
            Selector({"a"}).MatchingValue("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a") ==
              Selector({"a"}).MatchingValue("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingValue("a", false) ==
               Selector({"a"}).MatchingValue("a", true));
  EXPECT_LT(Selector({"a"}).MatchingValue("a", false),
            Selector({"a"}).MatchingValue("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a", true) ==
              Selector({"a"}).MatchingValue("a", true));
}

TEST(SelectorTest, Comparison_Frames) {
  Selector ab({"a", "b"});
  EXPECT_EQ(ab, ab);

  Selector cb({"c", "b"});
  EXPECT_FALSE(ab == cb);
  EXPECT_TRUE(ab < cb);
  EXPECT_FALSE(cb < ab);

  Selector b({"b"});
  EXPECT_FALSE(ab == b);
  EXPECT_TRUE(b < ab);
  EXPECT_FALSE(ab < b);
}

TEST(SelectorTest, Comparison_MultipleFilters) {
  Selector abcdef;
  abcdef.proto.add_filters()->set_css_selector("abc");
  abcdef.proto.add_filters()->set_css_selector("def");

  Selector abcdef2;
  abcdef2.proto.add_filters()->set_css_selector("abc");
  abcdef2.proto.add_filters()->set_css_selector("def");
  EXPECT_EQ(abcdef, abcdef2);
  EXPECT_FALSE(abcdef < abcdef2);
  EXPECT_FALSE(abcdef2 < abcdef);

  Selector defabc;
  defabc.proto.add_filters()->set_css_selector("def");
  defabc.proto.add_filters()->set_css_selector("abc");
  EXPECT_FALSE(abcdef == defabc);
  EXPECT_TRUE(abcdef < defabc);
  EXPECT_FALSE(defabc < abcdef);

  Selector abc;
  abc.proto.add_filters()->set_css_selector("abc");
  EXPECT_FALSE(abcdef == abc);
  EXPECT_TRUE(abc < abcdef);
  EXPECT_FALSE(abcdef < abc);
}

}  // namespace
}  // namespace autofill_assistant
