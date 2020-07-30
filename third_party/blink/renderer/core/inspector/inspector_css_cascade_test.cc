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
  EXPECT_EQ((unsigned)3, value_list->length());
  EXPECT_EQ("100px", value_list->Item(0).CssText());
  EXPECT_EQ("1fr", value_list->Item(1).CssText());
  EXPECT_EQ("20%", value_list->Item(2).CssText());
}

}  // namespace blink
