// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(NetDnsUtilTest, IsValidDoHTemplate) {
  std::string server_method;
  EXPECT_TRUE(IsValidDoHTemplate(
      "https://dnsserver.example.net/dns-query{?dns}", &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDoHTemplate(
      "https://dnsserver.example.net/dns-query{?dns,extra}", &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDoHTemplate(
      "https://dnsserver.example.net/dns-query{?query}", &server_method));
  EXPECT_EQ("POST", server_method);

  EXPECT_TRUE(IsValidDoHTemplate("https://dnsserver.example.net/dns-query",
                                 &server_method));
  EXPECT_EQ("POST", server_method);

  EXPECT_TRUE(IsValidDoHTemplate("https://query:{dns}@dnsserver.example.net",
                                 &server_method));
  EXPECT_EQ("GET", server_method);

  EXPECT_TRUE(IsValidDoHTemplate("https://dnsserver.example.net{/dns}",
                                 &server_method));
  EXPECT_EQ("GET", server_method);

  // Invalid template format
  EXPECT_FALSE(IsValidDoHTemplate(
      "https://dnsserver.example.net/dns-query{{?dns}}", &server_method));
  // Must be HTTPS
  EXPECT_FALSE(IsValidDoHTemplate("http://dnsserver.example.net/dns-query",
                                  &server_method));
  EXPECT_FALSE(IsValidDoHTemplate(
      "http://dnsserver.example.net/dns-query{?dns}", &server_method));
  // Template must expand to a valid URL
  EXPECT_FALSE(IsValidDoHTemplate("https://{?dns}", &server_method));
  // The hostname must not contain the dns variable
  EXPECT_FALSE(
      IsValidDoHTemplate("https://{dns}.dnsserver.net", &server_method));
}
