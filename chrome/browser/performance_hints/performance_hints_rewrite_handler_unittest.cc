// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_rewrite_handler.h"

#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

TEST(PerformanceHintsRewriteHandlerTest, ExtraQueryParams) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString(
          "www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?not=used&url=https://theactualurl.com/"
      "testpath?testquerytoo=true&unusedparamfromouterurl");
  base::Optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath?testquerytoo=true",
            result.value().spec());
}

TEST(PerformanceHintsRewriteHandlerTest, EscapedCharacters) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString(
          "www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?url=https://theactualurl.com/"
      "testpath?first=param%26second=param&unusedparamfromouterurl");
  base::Optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath?first=param&second=param",
            result.value().spec());
}

TEST(PerformanceHintsRewriteHandlerTest, NoMatchingParam) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString(
          "www.google.com/url?url");

  GURL url(
      "https://www.google.com/url?notactuallyurl=https://theactualurl.com");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(PerformanceHintsRewriteHandlerTest, InvalidUrl) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString(
          "www.google.com/url?url");

  GURL url("invalid");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(PerformanceHintsRewriteHandlerTest, EmptyConfig) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString("");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(PerformanceHintsRewriteHandlerTest, NoQueryParam) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString("www.google.com/url");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(PerformanceHintsRewriteHandlerTest, NoHostPath) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString("?url");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  ASSERT_FALSE(handler.HandleRewriteIfNecessary(url));
}

TEST(PerformanceHintsRewriteHandlerTest, HostOnly) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString("www.google.com/?url");

  GURL url("https://www.google.com?url=https://theactualurl.com/testpath");
  base::Optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());
}

TEST(PerformanceHintsRewriteHandlerTest, MultipleMatchers) {
  PerformanceHintsRewriteHandler handler =
      PerformanceHintsRewriteHandler::FromConfigString(
          "www.google.com/url?url,www.googleadservices.com/pagead/aclk?adurl");

  GURL url("https://www.google.com/url?url=https://theactualurl.com/testpath");
  base::Optional<GURL> result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());

  url = GURL(
      "https://www.googleadservices.com/pagead/aclk?adurl=https://"
      "theactualurl.com/testpath");
  result = handler.HandleRewriteIfNecessary(url);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("https://theactualurl.com/testpath", result.value().spec());
}
