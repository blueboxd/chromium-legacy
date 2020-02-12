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
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_select));
  EXPECT_EQ("ABC", ax_select->StringValue());
}

TEST_F(AXLayoutObjectTest, StringValueTextSecurity) {
  SetBodyInnerHTML(
      "<select id='t' style='-webkit-text-security:disc'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_select));
  // U+2022 -> \xE2\x80\xA2 in UTF-8
  EXPECT_EQ("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2",
            ax_select->StringValue().Utf8());
}

// Test if AX takes 'Retarget' described from
// https://dom.spec.whatwg.org/#retarget after hit-testing.
TEST_F(AXLayoutObjectTest, AccessibilityHitTest) {
  SetBodyInnerHTML(
      "<style>\
        .A{display:flex;flex:100%;margin-top:-37px;height:34px}\
        .B{display:flex;flex:1;flex-wrap:wrap}\
        .C{flex:100%;height:34px}\
      </style>\
      <div class='B'>\
      <div class='C'></div>\
      <input class='A' aria-label='Search' role='combobox'>\
      </div>");
  const AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  const IntPoint position(8, 5);
  AXObject* hit_test_result = ax_root->AccessibilityHitTest(position);
  EXPECT_NE(nullptr, hit_test_result);
  EXPECT_EQ(hit_test_result->RoleValue(),
            ax::mojom::Role::kTextFieldWithComboBox);
}

}  // namespace blink
