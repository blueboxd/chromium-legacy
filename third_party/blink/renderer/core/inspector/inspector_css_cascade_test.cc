// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_cascade.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class InspectorCSSCascadeTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorCSSCascadeTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
}

TEST_F(InspectorCSSCascadeTest, DirectlyMatchedRules) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #grid {
        display: grid;
        grid-gap: 10px;
        grid-template-columns: 100px 1fr 20%;
      }
    </style>
    <div id="grid">
    </div>
  )HTML");
  Element* grid = GetDocument().getElementById("grid");
  InspectorCSSCascade cascade(grid, kPseudoIdNone);
  const CSSValue* value =
      cascade.GetCascadedProperty(CSSPropertyID::kGridTemplateColumns);
  const CSSValueList* value_list = DynamicTo<CSSValueList>(value);
  EXPECT_EQ(3u, value_list->length());
  EXPECT_EQ("100px", value_list->Item(0).CssText());
  EXPECT_EQ("1fr", value_list->Item(1).CssText());
  EXPECT_EQ("20%", value_list->Item(2).CssText());
}

TEST_F(InspectorCSSCascadeTest, ParentRules) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #grid-container {
        display: inline-grid;
        grid-gap: 5px;
        grid-template-columns: 50px 1fr 10%;
      }
      #grid {
        display: grid;
        grid-gap: 10px;
        grid-template-columns: 100px 2fr 20%;
      }
    </style>
    <div id="grid-container">
      <div id="grid"></div>
    </div>
  )HTML");
  Element* grid = GetDocument().getElementById("grid");
  InspectorCSSCascade cascade(grid, kPseudoIdNone);
  HeapVector<Member<InspectorCSSMatchedRules>> parent_rules =
      cascade.ParentRules();
  Element* grid_container = GetDocument().getElementById("grid-container");
  // Some rules are coming for UA.
  EXPECT_EQ(3u, parent_rules.size());
  // grid_container is the first parent.
  EXPECT_EQ(grid_container, parent_rules.at(0)->element);
  // Some rules are coming from UA.
  EXPECT_EQ(2u, parent_rules.at(0)->matched_rules->size());
  auto rule = parent_rules.at(0)->matched_rules->at(1);
  EXPECT_EQ(
      "#grid-container { display: inline-grid; gap: 5px; "
      "grid-template-columns: 50px 1fr 10%; }",
      rule.first->cssText());
}

}  // namespace blink
