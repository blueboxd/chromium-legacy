// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/test_specifics_generator.h"
#include "chromeos/network/network_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace sync_wifi {

namespace {
const char kHexSsid[] = "0123456789ABCDEF";
}  // namespace

class NetworkIdentifierTest : public testing::Test {
 protected:
  NetworkIdentifierTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkIdentifierTest);
};

TEST_F(NetworkIdentifierTest, FromProto) {
  NetworkIdentifier id =
      NetworkIdentifier::FromProto(CreateSpecifics(kHexSsid));
  EXPECT_EQ(kHexSsid, id.hex_ssid());
  EXPECT_EQ(shill::kSecurityPsk, id.security_type());
}

TEST_F(NetworkIdentifierTest, FromNetwork) {
  chromeos::NetworkState network("dummy_path");
  network.PropertyChanged(shill::kWifiHexSsid, base::Value(kHexSsid));
  network.PropertyChanged(shill::kSecurityClassProperty,
                          base::Value(shill::kSecurityPsk));
  NetworkIdentifier id = NetworkIdentifier::FromNetwork(network);
  EXPECT_EQ(kHexSsid, id.hex_ssid());
  EXPECT_EQ(shill::kSecurityPsk, id.security_type());
}

TEST_F(NetworkIdentifierTest, FromString) {
  std::string string_id("0123456789ABCDEF_psk");
  NetworkIdentifier id = NetworkIdentifier::DeserializeFromString(string_id);
  EXPECT_EQ(kHexSsid, id.hex_ssid());
  EXPECT_EQ(shill::kSecurityPsk, id.security_type());
}

TEST_F(NetworkIdentifierTest, DifferentHexFormats) {
  NetworkIdentifier id("0x2f", shill::kSecurityPsk);
  EXPECT_EQ("2F", id.hex_ssid());

  id = NetworkIdentifier("0X2F", shill::kSecurityPsk);
  EXPECT_EQ("2F", id.hex_ssid());

  id = NetworkIdentifier("2f", shill::kSecurityPsk);
  EXPECT_EQ("2F", id.hex_ssid());
}

}  // namespace sync_wifi
