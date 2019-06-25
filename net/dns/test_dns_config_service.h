// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#ifndef NET_DNS_TEST_DNS_CONFIG_SERVICE_H_
#define NET_DNS_TEST_DNS_CONFIG_SERVICE_H_

namespace net {

// Simple test implementation of DnsConfigService that will trigger
// notifications only on explicitly calling On...() methods.
class TestDnsConfigService : public DnsConfigService {
 public:
  void ReadNow() override {}
  bool StartWatching() override;

  // Expose the protected methods to this test suite.
  void InvalidateConfig() { DnsConfigService::InvalidateConfig(); }

  void InvalidateHosts() { DnsConfigService::InvalidateHosts(); }

  void OnConfigRead(const DnsConfig& config) {
    DnsConfigService::OnConfigRead(config);
  }

  void OnHostsRead(const DnsHosts& hosts) {
    DnsConfigService::OnHostsRead(hosts);
  }

  void set_watch_failed(bool value) {
    DnsConfigService::set_watch_failed(value);
  }
};

}  // namespace net

#endif  // NET_DNS_TEST_DNS_CONFIG_SERVICE_H_
