// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(SmsParserTest, NoToken) {
  ASSERT_FALSE(SmsParser::Parse("foo"));
}

TEST(SmsParserTest, WithTokenInvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("To: foo"));
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("To:https://example.com"));
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("To: //example.com"));
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("To: ftp://example.com"));
}

TEST(SmsParserTest, HttpScheme) {
  ASSERT_FALSE(SmsParser::Parse("To: http://example.com"));
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("To: mailto:goto@chromium.org"));
}

TEST(SmsParserTest, Basic) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("To: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Realistic) {
  base::Optional<url::Origin> origin = SmsParser::Parse(
      "<#> Your OTP is 1234ABC.\nTo: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Paths) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("To: https://example.com/foobar");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Message) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nTo: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Whitespace) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nTo: https://example.com ");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Newlines) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nTo: https://example.com\n");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, TwoTokens) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("To: https://a.com To: https://b.com");

  GURL url("https://b.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, AppHash) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("<#> Hello World\nTo: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

}  // namespace content
