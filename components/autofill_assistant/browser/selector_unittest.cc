// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

TEST(SelectorTest, FromProto) {
  ElementReferenceProto proto;
  proto.add_selectors("a");
  proto.add_selectors("b");
  proto.set_inner_text_pattern("c");
  proto.set_inner_text_pattern_case_sensitive(true);
  proto.set_value_pattern("d");
  proto.set_value_pattern_case_sensitive(true);
  proto.set_visibility_requirement(MUST_BE_VISIBLE);
  proto.set_pseudo_type(PseudoType::BEFORE);

  Selector selector(proto);
  EXPECT_THAT(selector.selectors, testing::ElementsAre("a", "b"));
  EXPECT_TRUE(selector.must_be_visible);
  EXPECT_EQ("c", selector.inner_text_pattern);
  EXPECT_TRUE(selector.inner_text_pattern_case_sensitive);
  EXPECT_EQ("d", selector.value_pattern);
  EXPECT_TRUE(selector.value_pattern_case_sensitive);
  EXPECT_EQ(PseudoType::BEFORE, selector.pseudo_type);
}

TEST(SelectorTest, ToProto) {
  Selector selector;
  selector.selectors.emplace_back("a");
  selector.selectors.emplace_back("b");
  selector.inner_text_pattern = "c";
  selector.inner_text_pattern_case_sensitive = true;
  selector.value_pattern = "d";
  selector.value_pattern_case_sensitive = true;
  selector.must_be_visible = true;
  selector.pseudo_type = PseudoType::BEFORE;

  ElementReferenceProto proto = selector.ToElementReferenceProto();
  EXPECT_THAT(proto.selectors(), testing::ElementsAre("a", "b"));
  EXPECT_EQ("c", proto.inner_text_pattern());
  EXPECT_TRUE(proto.inner_text_pattern_case_sensitive());
  EXPECT_EQ("d", proto.value_pattern());
  EXPECT_TRUE(proto.value_pattern_case_sensitive());
  EXPECT_EQ(MUST_BE_VISIBLE, proto.visibility_requirement());
  EXPECT_EQ(PseudoType::BEFORE, proto.pseudo_type());
}

TEST(SelectorTest, Comparison) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_LT(Selector({"a"}), Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));

  EXPECT_FALSE(Selector({"a"}, PseudoType::BEFORE) ==
               Selector({"a"}, PseudoType::AFTER));
  EXPECT_LT(Selector({"a"}, PseudoType::BEFORE),
            Selector({"a"}, PseudoType::AFTER));
  EXPECT_LT(Selector({"a"}, PseudoType::BEFORE), Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}, PseudoType::BEFORE) ==
              Selector({"a"}, PseudoType::BEFORE));

  EXPECT_FALSE(Selector({"a"}) == Selector({"a"}).MustBeVisible());
  EXPECT_LT(Selector({"a"}), Selector({"a"}).MustBeVisible());
  EXPECT_TRUE(Selector({"a"}).MustBeVisible() ==
              Selector({"a"}).MustBeVisible());

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

}  // namespace
}  // namespace autofill_assistant
