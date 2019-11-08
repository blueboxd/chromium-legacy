// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_ACCESSIBILITY_H_
#define CONTENT_PUBLIC_TEST_BROWSER_ACCESSIBILITY_H_

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace content {

class AccessibilityTreeFormatter;

// A class that exposes BrowserAccessibility for testing in a controlled manner.
// Additional methods can be added to expose methods from BrowserAccessibility,
// as long as the methods do not expose non-public content/ classes.
class TestBrowserAccessibility : public ui::AXPlatformNodeDelegate {
 public:
  // Dumps a BrowserAccessibility tree into a string.
  static void FormatAccessibilityTree(AccessibilityTreeFormatter* formatter,
                                      TestBrowserAccessibility* root,
                                      base::string16* contents);

  ~TestBrowserAccessibility() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_ACCESSIBILITY_H_
