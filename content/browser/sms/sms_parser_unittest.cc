// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include <string>

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

url::Origin ParseOrigin(const std::string& message) {
  base::Optional<SmsParser::Result> result = SmsParser::Parse(message);
  if (!result)
    return url::Origin();
  return result->origin;
}

std::string ParseOTP(const std::string& message) {
  base::Optional<SmsParser::Result> result = SmsParser::Parse(message);
  if (!result)
    return "";
  return result->one_time_code;
}

}  // namespace

TEST(SmsParserTest, NoToken) {
  ASSERT_FALSE(SmsParser::Parse("foo"));
}

TEST(SmsParserTest, WithTokenInvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("@foo"));
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("@example.com#12345"));
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("@//example.com #123"));
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("@ftp://example.com #123"));
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("@mailto:goto@chromium.org #123"));
}

TEST(SmsParserTest, MissingOneTimeCodeParameter) {
  ASSERT_FALSE(SmsParser::Parse("@example.com"));
}

TEST(SmsParserTest, Basic) {
  auto result = SmsParser::Parse("@example.com #12345");

  ASSERT_TRUE(result);
  EXPECT_EQ("12345", result->one_time_code);
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")), result->origin);
}

TEST(SmsParserTest, Realistic) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("<#> Your OTP is 1234ABC.\n@example.com #12345"));
}

TEST(SmsParserTest, OneTimeCode) {
  EXPECT_EQ("123", ParseOTP("@example.com #123"));
}

TEST(SmsParserTest, LocalhostForDevelopment) {
  EXPECT_EQ(url::Origin::Create(GURL("http://localhost")),
            ParseOrigin("@localhost #123"));
  ASSERT_FALSE(SmsParser::Parse("@localhost:8080 #123"));
  ASSERT_FALSE(SmsParser::Parse("@localhost"));
}

TEST(SmsParserTest, Paths) {
  ASSERT_FALSE(SmsParser::Parse("@example.com/foobar #123"));
}

TEST(SmsParserTest, Message) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123"));
}

TEST(SmsParserTest, Whitespace) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123 "));
}

TEST(SmsParserTest, Dashes) {
  EXPECT_EQ(url::Origin::Create(GURL("https://web-otp-example.com")),
            ParseOrigin("@web-otp-example.com #123"));
}

TEST(SmsParserTest, Newlines) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123\n"));
}

TEST(SmsParserTest, TwoTokens) {
  EXPECT_EQ(url::Origin::Create(GURL("https://b.com")),
            ParseOrigin("@a.com @b.com #123"));
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("@a.com #123 @b.com"));
}

TEST(SmsParserTest, Ports) {
  ASSERT_FALSE(SmsParser::Parse("@a.com:8443 #123"));
}

TEST(SmsParserTest, Username) {
  ASSERT_FALSE(SmsParser::Parse("@username@a.com #123"));
}

TEST(SmsParserTest, QueryParams) {
  ASSERT_FALSE(SmsParser::Parse("@a.com/?foo=123 #123"));
}

TEST(SmsParserTest, HarmlessOriginsButInvalid) {
  ASSERT_FALSE(SmsParser::Parse("@data://123"));
}

TEST(SmsParserTest, AppHash) {
  EXPECT_EQ(
      url::Origin::Create(GURL("https://example.com")),
      ParseOrigin("<#> Hello World\nApp Hash: s3LhKBB0M33\n@example.com #123"));
}

TEST(SmsParserTest, OneTimeCodeCharRanges) {
  EXPECT_EQ("cannot-contain-hashes",
            ParseOTP("@example.com #cannot-contain-hashes#yes"));
  EXPECT_EQ("can-contain-numbers-like-123",
            ParseOTP("@example.com #can-contain-numbers-like-123"));
  EXPECT_EQ("can-contain-utf8-like-🤷",
            ParseOTP("@example.com #can-contain-utf8-like-🤷"));
  EXPECT_EQ("can-contain-chars-like-*^$@",
            ParseOTP("@example.com #can-contain-chars-like-*^$@"));
  EXPECT_EQ("human-readable-words-like-sillyface",
            ParseOTP("@example.com #human-readable-words-like-sillyface"));
  EXPECT_EQ("can-it-be-super-lengthy-like-a-lot",
            ParseOTP("@example.com #can-it-be-super-lengthy-like-a-lot"));
  EXPECT_EQ("otp", ParseOTP("@example.com #otp with space"));
}

}  // namespace content
