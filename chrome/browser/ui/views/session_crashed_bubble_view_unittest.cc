// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/test/views/chrome_views_test_base.h"

using SessionCrashedBubbleViewUnitTest = ChromeViewsTestBase;

TEST_F(SessionCrashedBubbleViewUnitTest, BubbleHasCloseButton) {
  views::View anchor;
  SessionCrashedBubbleView bubble(&anchor, nullptr, true);
  EXPECT_TRUE(bubble.ShouldShowCloseButton());
}
