// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CSSPropertyParserHelpersTest, ParseRevert) {
  {
    ScopedCSSRevertForTest scoped_revert(true);
    EXPECT_TRUE(css_property_parser_helpers::IsCSSWideKeyword("revert"));
  }

  {
    ScopedCSSRevertForTest scoped_revert(false);
    EXPECT_FALSE(css_property_parser_helpers::IsCSSWideKeyword("revert"));
  }
}

}  // namespace blink
