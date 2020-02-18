// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DOH_PROVIDER_LIST_H_
#define NET_DNS_PUBLIC_DOH_PROVIDER_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "net/base/ip_address.h"
#include "net/base/net_export.h"

namespace net {

// Represents insecure DNS, DoT, and DoH services run by the same provider.
// These entries are used to support upgrade from insecure DNS or DoT services
// to associated DoH services in automatic mode and to populate the dropdown
// menu for secure mode. To be eligible for auto-upgrade, entries must have a
// non-empty |ip_strs| or non-empty |dns_over_tls_hostnames|. To be eligible for
// the dropdown menu, entries must have non-empty |ui_name| and
// |privacy_policy|.
struct NET_EXPORT DohProviderEntry {
  DohProviderEntry(std::string provider,
                   std::set<std::string> ip_strs,
                   std::set<std::string> dns_over_tls_hostnames,
                   std::string dns_over_https_template,
                   std::string ui_name,
                   std::string privacy_policy);
  DohProviderEntry(const DohProviderEntry& other);
  ~DohProviderEntry();

  const std::string provider;
  std::set<IPAddress> ip_addresses;
  const std::set<std::string> dns_over_tls_hostnames;
  const std::string dns_over_https_template;
  const std::string ui_name;
  const std::string privacy_policy;
};

// Returns the full list of DoH providers. A subset of this list may be used
// to support upgrade in automatic mode or to populate the dropdown menu for
// secure mode.
NET_EXPORT const std::vector<DohProviderEntry>& GetDohProviderList();

}  // namespace net

#endif  // NET_DNS_PUBLIC_DOH_PROVIDER_LIST_H_
