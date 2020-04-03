// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

TEST(CSSParsingUtilsTest, BasicShapeUseCount) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSBasicShape;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>span { shape-outside: circle(); }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST(CSSParsingUtilsTest, Revert) {
  {
    ScopedCSSRevertForTest scoped_revert(true);
    EXPECT_TRUE(css_parsing_utils::IsCSSWideKeyword(CSSValueID::kRevert));
  }

  {
    ScopedCSSRevertForTest scoped_revert(false);
    EXPECT_FALSE(css_parsing_utils::IsCSSWideKeyword(CSSValueID::kRevert));
  }
}

}  // namespace blink
