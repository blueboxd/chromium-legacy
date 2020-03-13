// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace lookalikes {

// These redirects are safe:
// - http[s]://sité.test -> http[s]://site.test
// - http[s]://sité.test/path -> http[s]://site.test
// - http[s]://subdomain.sité.test -> http[s]://site.test
// - http[s]://random.test -> http[s]://sité.test -> http[s]://site.test
// - http://sité.test/path -> https://sité.test/path -> https://site.test ->
// <any_url>
// - "subdomain" on either side.
//
// These are not safe:
// - http[s]://[subdomain.]sité.test -> http[s]://[subdomain.]site.test/path
// because the redirected URL has a path.
TEST(LookalikeUrlNavigationThrottleTest, IsSafeRedirect) {
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com"), GURL("http://subdomain.example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com"),
                      GURL("https://example.com")}));
  // Original site redirects to HTTPS.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("https://éxample.com"),
                      GURL("https://example.com")}));
  // Original site redirects to HTTPS which redirects to HTTP which redirects
  // back to HTTPS of the non-IDN version.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com/redir1"), GURL("https://éxample.com/redir1"),
       GURL("http://éxample.com/redir2"), GURL("https://example.com/")}));
  // Same as above, but there is another redirect at the end of the chain.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com/redir1"), GURL("https://éxample.com/redir1"),
       GURL("http://éxample.com/redir2"), GURL("https://example.com/"),
       GURL("https://totallydifferentsite.com/somepath")}));

  // Not a redirect, the chain is too short.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com")}));
  // Not safe: Redirected site is not the same as the matched site.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com"),
                                              GURL("http://other-site.com")}));
  // Not safe: Initial URL doesn't redirect to the root of the suggested domain.
  EXPECT_FALSE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com"), GURL("http://example.com/path")}));
  // Not safe: The first redirect away from éxample.com is not to the matching
  // non-IDN site.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com"),
                                              GURL("http://intermediate.com"),
                                              GURL("http://example.com")}));

  // Not safe: The redirect stays unsafe from éxample.com to éxample.com.
  EXPECT_FALSE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://éxample.com")}));
  // Not safe: Same, but to a path on the bad domain
  EXPECT_FALSE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com"), GURL("http://éxample.com/path")}));
  // Not safe: Same, but with an intermediary domain.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com/path"),
                                              GURL("http://intermediate.com/p"),
                                              GURL("http://éxample.com/dir")}));
}

}  // namespace lookalikes
