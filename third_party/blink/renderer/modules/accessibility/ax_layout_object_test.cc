// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

class AXLayoutObjectTest : public test::AccessibilityTest {};

TEST_F(AXLayoutObjectTest, StringValueTextTransform) {
  SetBodyInnerHTML(
      "<select id='t' style='text-transform:uppercase'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(ax_select->IsAXLayoutObject());
  EXPECT_EQ("ABC", ax_select->StringValue());
}

TEST_F(AXLayoutObjectTest, StringValueTextSecurity) {
  SetBodyInnerHTML(
      "<select id='t' style='-webkit-text-security:disc'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(ax_select->IsAXLayoutObject());
  // U+2022 -> \xE2\x80\xA2 in UTF-8
  EXPECT_EQ("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2",
            ax_select->StringValue().Utf8());
}

}  // namespace blink
