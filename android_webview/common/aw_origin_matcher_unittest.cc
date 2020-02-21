// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_origin_matcher.h"

#include "base/lazy_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace android_webview {

namespace {
// WebView's special configuration for parsing custom scheme.
class InitOnce {
 public:
  InitOnce() {
    // Non-standand schemes support for WebView.
    url::EnableNonStandardSchemesForAndroidWebView();
  }
};

base::LazyInstance<InitOnce>::Leaky g_once = LAZY_INSTANCE_INITIALIZER;

void EnsureConfigured() {
  g_once.Get();
}

}  // namespace

class AwOriginMatcherTest : public testing::Test {
 public:
  void SetUp() override { EnsureConfigured(); }

  static url::Origin CreateOriginFromString(const std::string& url) {
    return url::Origin::Create(GURL(url));
  }
};

TEST_F(AwOriginMatcherTest, InvalidInputs) {
  AwOriginMatcher matcher;
  // Empty string is invalid.
  EXPECT_FALSE(matcher.AddRuleFromString(""));
  // Scheme doesn't present.
  EXPECT_FALSE(matcher.AddRuleFromString("example.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("://example.com"));
  // Scheme doesn't do wildcard matching.
  EXPECT_FALSE(matcher.AddRuleFromString("*://example.com"));
  // URL like rule is invalid.
  EXPECT_FALSE(matcher.AddRuleFromString("https://www.example.com/index.html"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://192.168.0.1/*"));
  // Only accept hostname pattern starts with "*." if there is a "*" inside.
  EXPECT_FALSE(matcher.AddRuleFromString("https://*foobar.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://x.*.y.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://*example.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://e*xample.com"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://*"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://*"));
  // Invalid port.
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:**"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:-1"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://example.com:+443"));
  // Empty hostname pattern for http/https.
  EXPECT_FALSE(matcher.AddRuleFromString("http://"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://:80"));
  // No IP block support.
  EXPECT_FALSE(matcher.AddRuleFromString("https://192.168.0.0/16"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13::abc/33"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://:1"));
  // Invalid IP address.
  EXPECT_FALSE(matcher.AddRuleFromString("http://[a:b:*]"));
  EXPECT_FALSE(matcher.AddRuleFromString("http://[a:b:*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13::*"));
  EXPECT_FALSE(matcher.AddRuleFromString("https://fefe:13:*/33"));
  // Custom scheme with host and/or port are invalid. This is because in
  // WebView, all the URI with the same custom scheme belong to one origin.
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://hostname:80"));
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://hostname"));
  EXPECT_FALSE(matcher.AddRuleFromString("x-mail://*"));
  // file scheme with "host"
  EXPECT_FALSE(matcher.AddRuleFromString("file://host"));
  EXPECT_FALSE(matcher.AddRuleFromString("file://*"));
}

TEST_F(AwOriginMatcherTest, ExactMatching) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://www.example.com:99"));
  EXPECT_EQ("https://www.example.com:99", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(AwOriginMatcherTest, SchemeDefaultPortHttp) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://www.example.com"));
  EXPECT_EQ("http://www.example.com:80", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:80")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://music.example.com:80")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://music.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:80")));
}

TEST_F(AwOriginMatcherTest, SchemeDefaultPortHttps) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://www.example.com"));
  EXPECT_EQ("https://www.example.com:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:443")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:443")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(AwOriginMatcherTest, SubdomainMatching) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://*.example.com"));
  EXPECT_EQ("https://*.example.com:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:443")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://music.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:443")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://music.video.radio.example.com")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("ftp://www.example.com")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("https://example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(AwOriginMatcherTest, SubdomainMatching2) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://*.www.example.com"));
  EXPECT_EQ("http://*.www.example.com:80", matcher.rules()[0]->ToString());

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://abc.www.example.com:80")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("http://music.video.www.example.com")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("ftp://www.example.com")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("https://example.com")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://www.example.com:99")));
  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://music.example.com:99")));
}

TEST_F(AwOriginMatcherTest, PunyCode) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("http://*.xn--fsqu00a.com"));

  // Chinese domain example.com
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("http://www.例子.com")));
}

TEST_F(AwOriginMatcherTest, IPv4AddressMatching) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://192.168.0.1"));
  EXPECT_EQ("https://192.168.0.1:443", matcher.rules()[0]->ToString());

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("https://192.168.0.1")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://192.168.0.1:443")));

  EXPECT_FALSE(
      matcher.Matches(CreateOriginFromString("https://192.168.0.1:99")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("http://192.168.0.1")));
  EXPECT_FALSE(matcher.Matches(CreateOriginFromString("http://192.168.0.2")));
}

TEST_F(AwOriginMatcherTest, IPv6AddressMatching) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("https://[3ffe:2a00:100:7031:0:0::1]"));
  // Note that the IPv6 address is canonicalized.
  EXPECT_EQ("https://[3ffe:2a00:100:7031::1]:443",
            matcher.rules()[0]->ToString());

  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]")));
  EXPECT_TRUE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]:443")));

  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("http://[3ffe:2a00:100:7031::1]")));
  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("http://[3ffe:2a00:100:7031::1]:443")));
  EXPECT_FALSE(matcher.Matches(
      CreateOriginFromString("https://[3ffe:2a00:100:7031::1]:8080")));
}

TEST_F(AwOriginMatcherTest, WildcardMatchesEveryOrigin) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("*"));

  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://foo.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://www.example.com:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("http://192.168.0.1")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("http://192.168.0.1:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("https://[a:b:c:d::]")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("https://[a:b:c:d::]:8080")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("ftp://example.com")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("about:blank")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString(
      "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("file:///usr/local/a.txt")));
  EXPECT_TRUE(matcher.Matches(CreateOriginFromString(
      "blob:http://127.0.0.1:8080/0530b9d1-c1c2-40ff-9f9c-c57336646baa")));
}

TEST_F(AwOriginMatcherTest, FileOrigin) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("file://"));

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("file:///sdcard")));
  EXPECT_TRUE(
      matcher.Matches(CreateOriginFromString("file:///android_assets")));
}

TEST_F(AwOriginMatcherTest, CustomSchemeOrigin) {
  AwOriginMatcher matcher;
  EXPECT_TRUE(matcher.AddRuleFromString("x-mail://"));

  EXPECT_TRUE(matcher.Matches(CreateOriginFromString("x-mail://hostname")));
}

}  // namespace android_webview
