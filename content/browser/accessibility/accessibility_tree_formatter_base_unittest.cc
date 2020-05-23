// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

#include <iostream>

namespace content {

class AccessibilityTreeFormatterBaseTest : public testing::Test {
 public:
  AccessibilityTreeFormatterBaseTest() = default;
  ~AccessibilityTreeFormatterBaseTest() override = default;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override {
    test_browser_accessibility_delegate_ =
        std::make_unique<TestBrowserAccessibilityDelegate>();
  }

  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeFormatterBaseTest);
};

void ParseAndCheck(const char* input, const char* expected) {
  auto got = PropertyNode::FromProperty(base::UTF8ToUTF16(input)).ToString();
  EXPECT_EQ(got, expected);
}

TEST_F(AccessibilityTreeFormatterBaseTest, ParseProperty) {
  ParseAndCheck("Role", "Role");
  ParseAndCheck("ChildAt(3)", "ChildAt(3)");
  ParseAndCheck("Cell(3, 4)", "Cell(3, 4)");
  ParseAndCheck("Volume(3, 4, 5)", "Volume(3, 4, 5)");
  ParseAndCheck("TableFor(CellBy(id))", "TableFor(CellBy(id))");
  ParseAndCheck("A(B(1), 2)", "A(B(1), 2)");
  ParseAndCheck("A(B(1), 2, C(3, 4))", "A(B(1), 2, C(3, 4))");
  ParseAndCheck("[3, 4]", "[](3, 4)");
  ParseAndCheck("Cell([3, 4])", "Cell([](3, 4))");

  ParseAndCheck("Role(3", "Role(3)");
  ParseAndCheck("TableFor(CellBy(id", "TableFor(CellBy(id))");
  ParseAndCheck("[3, 4", "[](3, 4)");
}

}  // namespace content
