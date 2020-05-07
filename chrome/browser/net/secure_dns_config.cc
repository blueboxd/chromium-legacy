// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_config.h"

// static
constexpr char SecureDnsConfig::kModeOff[];
constexpr char SecureDnsConfig::kModeAutomatic[];
constexpr char SecureDnsConfig::kModeSecure[];

SecureDnsConfig::SecureDnsConfig(
    net::DnsConfig::SecureDnsMode mode,
    std::vector<net::DnsOverHttpsServerConfig> servers,
    ManagementMode management_mode)
    : mode_(mode),
      servers_(std::move(servers)),
      management_mode_(management_mode) {}
SecureDnsConfig::SecureDnsConfig(SecureDnsConfig&& other) = default;
SecureDnsConfig& SecureDnsConfig::operator=(SecureDnsConfig&& other) = default;
SecureDnsConfig::~SecureDnsConfig() = default;

// static
base::Optional<net::DnsConfig::SecureDnsMode> SecureDnsConfig::ParseMode(
    base::StringPiece name) {
  if (name == kModeSecure) {
    return net::DnsConfig::SecureDnsMode::SECURE;
  } else if (name == kModeAutomatic) {
    return net::DnsConfig::SecureDnsMode::AUTOMATIC;
  } else if (name == kModeOff) {
    return net::DnsConfig::SecureDnsMode::OFF;
  }
  return base::nullopt;
}

// static
const char* SecureDnsConfig::ModeToString(net::DnsConfig::SecureDnsMode mode) {
  switch (mode) {
    case net::DnsConfig::SecureDnsMode::SECURE:
      return kModeSecure;
    case net::DnsConfig::SecureDnsMode::AUTOMATIC:
      return kModeAutomatic;
    case net::DnsConfig::SecureDnsMode::OFF:
      return kModeOff;
  }
}
