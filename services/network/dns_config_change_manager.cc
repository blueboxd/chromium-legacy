// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/dns_config_change_manager.h"

#include <utility>

namespace network {

DnsConfigChangeManager::DnsConfigChangeManager() {
  net::NetworkChangeNotifier::AddDNSObserver(this);
}

DnsConfigChangeManager::~DnsConfigChangeManager() {
  net::NetworkChangeNotifier::RemoveDNSObserver(this);
}

void DnsConfigChangeManager::AddReceiver(
    mojo::PendingReceiver<mojom::DnsConfigChangeManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DnsConfigChangeManager::RequestNotifications(
    mojom::DnsConfigChangeManagerClientPtr client) {
  clients_.AddPtr(std::move(client));
}

void DnsConfigChangeManager::OnDNSChanged() {
  clients_.ForAllPtrs([](mojom::DnsConfigChangeManagerClient* client) {
    client->OnSystemDnsConfigChanged();
  });
}

void DnsConfigChangeManager::OnInitialDNSConfigRead() {
  // Service API makes no distinction between initial read and change.
  OnDNSChanged();
}

}  // namespace network
