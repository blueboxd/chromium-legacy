// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
#define CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_

#include <vector>

#include "base/optional.h"
#include "net/dns/dns_config.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

class PrefService;

namespace chrome_browser_net {
enum class SecureDnsUiManagementMode;
}  // namespace chrome_browser_net

// Retriever for Chrome configuration for the built-in DNS stub resolver.
class StubResolverConfigReader {
 public:
  // |local_state| must outlive the created reader.
  explicit StubResolverConfigReader(PrefService* local_state);

  StubResolverConfigReader(const StubResolverConfigReader&) = delete;
  StubResolverConfigReader& operator=(const StubResolverConfigReader&) = delete;

  // Returns the current host resolver configuration. |forced_management_mode|
  // is an optional param that will be set to indicate the type of override
  // applied by Chrome if provided.
  void GetConfiguration(
      bool* insecure_stub_resolver_enabled,
      net::DnsConfig::SecureDnsMode* secure_dns_mode,
      base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
          dns_over_https_servers,
      bool record_metrics = false,
      chrome_browser_net::SecureDnsUiManagementMode* forced_management_mode =
          nullptr);

  // Returns true if there are any active machine level policies or if the
  // machine is domain joined. This special logic is used to disable DoH by
  // default for Desktop platforms (the enterprise policy field
  // default_for_enterprise_users only applies to ChromeOS). We don't attempt
  // enterprise detection on Android at this time.
  bool ShouldDisableDohForManaged();

  // Returns true if there are parental controls detected on the device.
  bool ShouldDisableDohForParentalControls();

 private:
  PrefService* local_state_;
};

#endif  // CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
