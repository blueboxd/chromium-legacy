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
  ASSERT_FALSE(SmsParser::Parse("For: foo"));
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("To:https://example.com"));
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("For: //example.com"));
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("For: ftp://example.com"));
}

TEST(SmsParserTest, HttpScheme) {
  ASSERT_FALSE(SmsParser::Parse("For: http://example.com"));
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("For: mailto:goto@chromium.org"));
}

TEST(SmsParserTest, Basic) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("For: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Realistic) {
  base::Optional<url::Origin> origin = SmsParser::Parse(
      "<#> Your OTP is 1234ABC.\nFor: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Paths) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("For: https://example.com/foobar");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Message) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFor: https://example.com");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Whitespace) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFor: https://example.com ");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, Newlines) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("hello world\nFor: https://example.com\n");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, TwoTokens) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("For: https://a.com For: https://b.com");

  GURL url("https://b.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

TEST(SmsParserTest, DifferentPorts) {
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com:8443/");

    GURL url("https://a.com");
    ASSERT_NE(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com:8443/");

    GURL url("https://a.com:443");
    ASSERT_NE(origin, url::Origin::Create(url));
  }
}

TEST(SmsParserTest, ImplicitPort) {
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com:443/");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com:8443/");

    GURL url("https://a.com");
    ASSERT_NE(origin, url::Origin::Create(url));
  }
}

TEST(SmsParserTest, Redirector) {
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com/redirect?https://b.com");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com/redirect?https:%2f%2fb.com");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://a.com/redirect#https:%2f%2fb.com");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
}

TEST(SmsParserTest, UsernameAndPassword) {
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://b.com@a.com/");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://b.com:c.com@a.com/");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
  {
    base::Optional<url::Origin> origin =
        SmsParser::Parse("For: https://b.com:noodle@a.com:443/");

    GURL url("https://a.com");
    ASSERT_EQ(origin, url::Origin::Create(url));
  }
}

TEST(SmsParserTest, AppHash) {
  base::Optional<url::Origin> origin =
      SmsParser::Parse("<#> Hello World\nFor: https://example.com?s3LhKBB0M33");

  GURL url("https://example.com");
  ASSERT_EQ(origin, url::Origin::Create(url));
}

}  // namespace content
