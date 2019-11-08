// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/browser_accessibility.h"

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

namespace content {

// static
void TestBrowserAccessibility::FormatAccessibilityTree(
    AccessibilityTreeFormatter* formatter,
    TestBrowserAccessibility* root,
    base::string16* contents) {
  auto* formatter_internal =
      static_cast<AccessibilityTreeFormatterBase*>(formatter);
  BrowserAccessibility* root_internal = FromTestBrowserAccessibility(root);
  formatter_internal->FormatAccessibilityTreeForTesting(root_internal,
                                                        contents);
}

TestBrowserAccessibility* ToTestBrowserAccessibility(
    BrowserAccessibility* browser_accessibility) {
  return reinterpret_cast<TestBrowserAccessibility*>(browser_accessibility);
}

BrowserAccessibility* FromTestBrowserAccessibility(
    TestBrowserAccessibility* test_browser_accessibility) {
  return reinterpret_cast<BrowserAccessibility*>(test_browser_accessibility);
}

}  // namespace content
