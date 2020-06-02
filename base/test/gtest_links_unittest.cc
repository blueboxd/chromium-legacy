// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_links.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(GtestLinksTest, AddInvalidLink) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH(AddLinkToTestResult("unique_link", "invalid`"), "");
#endif
}

TEST(GtestLinksTest, AddInvalidName) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH(AddLinkToTestResult("invalid-name", "http://google.com"), "");
#endif
}

TEST(GtestLinksTest, AddValidLink) {
  AddLinkToTestResult("name", "http://google.com");
}

}  // namespace base
