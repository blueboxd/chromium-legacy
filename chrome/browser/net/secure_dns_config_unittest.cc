// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_config.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SecureDnsConfig, ParseMode) {
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF,
            SecureDnsConfig::ParseMode("off").value());
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::AUTOMATIC,
            SecureDnsConfig::ParseMode("automatic").value());
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::SECURE,
            SecureDnsConfig::ParseMode("secure").value());

  EXPECT_FALSE(SecureDnsConfig::ParseMode("foo").has_value());
  EXPECT_FALSE(SecureDnsConfig::ParseMode("").has_value());
}

TEST(SecureDnsConfig, ModeToString) {
  EXPECT_EQ(std::string("off"),
            SecureDnsConfig::ModeToString(net::DnsConfig::SecureDnsMode::OFF));
  EXPECT_EQ(
      std::string("automatic"),
      SecureDnsConfig::ModeToString(net::DnsConfig::SecureDnsMode::AUTOMATIC));
  EXPECT_EQ(std::string("secure"), SecureDnsConfig::ModeToString(
                                       net::DnsConfig::SecureDnsMode::SECURE));
}

TEST(SecureDnsConfig, Constructor) {
  std::vector<net::DnsOverHttpsServerConfig> servers{
      {{"https://template1", false}, {"https://template2", true}}};
  SecureDnsConfig config(
      net::DnsConfig::SecureDnsMode::SECURE, servers,
      SecureDnsConfig::ManagementMode::kDisabledParentalControls);
  EXPECT_EQ(net::DnsConfig::SecureDnsMode::SECURE, config.mode());
  EXPECT_THAT(config.servers(), testing::ElementsAreArray(servers));
  EXPECT_EQ(SecureDnsConfig::ManagementMode::kDisabledParentalControls,
            config.management_mode());
}
