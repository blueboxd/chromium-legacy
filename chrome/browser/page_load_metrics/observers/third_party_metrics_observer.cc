// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

ThirdPartyMetricsObserver::CookieAccessTypes::CookieAccessTypes(
    AccessType access_type) {
  switch (access_type) {
    case AccessType::kRead:
      read = true;
      break;
    case AccessType::kWrite:
      write = true;
      break;
  }
}

ThirdPartyMetricsObserver::ThirdPartyMetricsObserver() = default;
ThirdPartyMetricsObserver::~ThirdPartyMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  RecordMetrics();
  return STOP_OBSERVING;
}

void ThirdPartyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordMetrics();
}

void ThirdPartyMetricsObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  OnCookieAccess(url, first_party_url, blocked_by_policy, AccessType::kRead);
}

void ThirdPartyMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  OnCookieAccess(url, first_party_url, blocked_by_policy, AccessType::kWrite);
}

void ThirdPartyMetricsObserver::OnCookieAccess(const GURL& url,
                                               const GURL& first_party_url,
                                               bool blocked_by_policy,
                                               AccessType access_type) {
  if (blocked_by_policy) {
    page_has_blocked_cookies_ = true;
    return;
  }

  // TODO(csharrison): Optimize the domain lookup.
  if (!url.is_valid() ||
      net::registry_controlled_domains::SameDomainOrHost(
          url, first_party_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return;
  }

  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (registrable_domain.empty())
    return;

  auto it = third_party_access_types_.find(registrable_domain);

  if (it != third_party_access_types_.end()) {
    switch (access_type) {
      case AccessType::kRead:
        it->second.read = true;
        break;
      case AccessType::kWrite:
        it->second.write = true;
        break;
    }
    return;
  }

  // Don't let the map grow unbounded.
  if (third_party_access_types_.size() >= 1000)
    return;

  third_party_access_types_.emplace(registrable_domain, access_type);
}

void ThirdPartyMetricsObserver::RecordMetrics() {
  if (page_has_blocked_cookies_)
    return;

  int origin_reads = 0;
  int origin_writes = 0;
  for (auto it : third_party_access_types_) {
    origin_reads += it.second.read;
    origin_writes += it.second.write;
  }
  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.Read",
                            origin_reads);
  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.Write",
                            origin_writes);
}
