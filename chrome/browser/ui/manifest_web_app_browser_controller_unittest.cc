// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/manifest_web_app_browser_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ManifestWebAppBrowserControllerTest, IsInScope) {
  struct TestCase {
    const char* scope;
    const char* url;
    bool is_in_scope;
  };

  const TestCase kTestCases[]{
      {"https://www.foo.com/", "https://www.foo.com/", true},
      {"https://www.foo.com/", "https://www.foo.com/bar/", true},
      {"https://www.foo.com/", "https://www.foo.com/bar/fnord.asp", true},
      {"https://www.foo.com/", "https://www.foo.com/bar?baz=2", true},
      {"https://www.foo.com/", "https://www.foo.com/bar#foobar", true},
      {"https://www.foo.com/x/", "https://www.foo.com/x/", true},
      {"https://www.foo.com/x/", "https://www.foo.com/x/bar/", true},
      {"https://www.foo.com/x/", "https://www.foo.com/x/bar/fnord.asp", true},
      {"https://www.foo.com/x/", "https://www.foo.com/x/bar?baz=2", true},
      {"https://www.foo.com/x/", "https://www.foo.com/x/bar#foobar", true},
      {"https://www.foo.com/x", "https://www.foo.com/x", true},
      {"https://www.foo.com/x", "https://www.foo.com/x/", true},
      {"https://www.foo.com/x", "https://www.foo.com/x/bar/", true},
      {"https://www.foo.com/x", "https://www.foo.com/x/bar/fnord.asp", true},
      {"https://www.foo.com/x", "https://www.foo.com/x/bar?baz=2", true},
      {"https://www.foo.com/x", "https://www.foo.com/x/bar#foobar", true},
      {"https://www.foo.com/x/", "https://www.foo.com/xavier/", false},
      {"https://www.foo.com/x/", "https://www.foo.com/", false},
      {"https://www.foo.com/x/", "https://www.foo.com/y/", false},
      {"https://www.foo.com/x/", "https://web.foo.com/x/", false},
      {"https://www.foo.com/x/", "http://www.foo.com/x/", false},
      {"https://www.foo.com/x/", "http://www.foo.com:123/x/", false},
  };

  for (const TestCase test_case : kTestCases) {
    EXPECT_EQ(test_case.is_in_scope,
              ManifestWebAppBrowserController::IsInScope(GURL(test_case.url),
                                                         GURL(test_case.scope)))
        << "URL: " << test_case.url << " SCOPE: " << test_case.scope;
  }
}
