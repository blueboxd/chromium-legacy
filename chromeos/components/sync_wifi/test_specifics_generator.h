// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_SPECIFICS_GENERATOR_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_SPECIFICS_GENERATOR_H_

namespace sync_wifi {

namespace {

sync_pb::WifiConfigurationSpecificsData CreateSpecifics(
    const std::string& ssid) {
  sync_pb::WifiConfigurationSpecificsData specifics;
  specifics.set_hex_ssid(ssid);
  specifics.set_security_type(
      sync_pb::WifiConfigurationSpecificsData::SECURITY_TYPE_PSK);
  specifics.set_passphrase("password");
  specifics.set_automatically_connect(
      sync_pb::WifiConfigurationSpecificsData::AUTOMATICALLY_CONNECT_ENABLED);
  specifics.set_is_preferred(
      sync_pb::WifiConfigurationSpecificsData::IS_PREFERRED_ENABLED);
  specifics.set_metered(
      sync_pb::WifiConfigurationSpecificsData::METERED_OPTION_AUTO);
  sync_pb::WifiConfigurationSpecificsData_ProxyConfiguration proxy_config;
  proxy_config.set_proxy_option(sync_pb::WifiConfigurationSpecificsData::
                                    ProxyConfiguration::PROXY_OPTION_DISABLED);
  specifics.mutable_proxy_configuration()->CopyFrom(proxy_config);
  return specifics;
}

}  // namespace

}  // namespace sync_wifi

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_TEST_SPECIFICS_GENERATOR_H_
