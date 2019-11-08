// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BROWSER_ACCESSIBILITY_H_
#define CONTENT_TEST_BROWSER_ACCESSIBILITY_H_

#include "content/public/test/browser_accessibility.h"

namespace content {

class BrowserAccessibility;

// Helper functions that are implementation details that should not be exposed
// via content/public/test/browser_accessibility.h.
TestBrowserAccessibility* ToTestBrowserAccessibility(
    BrowserAccessibility* browser_accessibility);
BrowserAccessibility* FromTestBrowserAccessibility(
    TestBrowserAccessibility* test_browser_accessibility);

}  // namespace content

#endif  // CONTENT_TEST_BROWSER_ACCESSIBILITY_H_
