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

ThirdPartyMetricsObserver::StorageAccessTypes::StorageAccessTypes(
    StorageType storage_type) {
  switch (storage_type) {
    case StorageType::kLocalStorage:
      local_storage = true;
      break;
    case StorageType::kSessionStorage:
      session_storage = true;
      break;
  }
}

ThirdPartyMetricsObserver::ThirdPartyMetricsObserver() = default;
ThirdPartyMetricsObserver::~ThirdPartyMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  RecordMetrics();
  return STOP_OBSERVING;
}

void ThirdPartyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
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

void ThirdPartyMetricsObserver::OnDomStorageAccessed(
    const GURL& url,
    const GURL& first_party_url,
    bool local,
    bool blocked_by_policy) {
  if (blocked_by_policy) {
    // When 3p DOM storage access is blocked, it must be that the "block third
    // party cookies" setting is set. In this case we don't want to record any
    // metrics for the page.
    should_record_metrics_ = false;
    return;
  }
  url::Origin origin = url::Origin::Create(url);
  if (origin.IsSameOriginWith(url::Origin::Create(first_party_url)))
    return;

  auto it = third_party_storage_access_types_.find(origin);

  if (it != third_party_storage_access_types_.end()) {
    if (local)
      it->second.local_storage = true;
    else
      it->second.session_storage = true;
    return;
  }

  // Don't let the map grow unbounded.
  if (third_party_storage_access_types_.size() >= 1000)
    return;

  third_party_storage_access_types_.emplace(
      origin,
      local ? StorageType::kLocalStorage : StorageType::kSessionStorage);
}

void ThirdPartyMetricsObserver::OnCookieAccess(const GURL& url,
                                               const GURL& first_party_url,
                                               bool blocked_by_policy,
                                               AccessType access_type) {
  if (blocked_by_policy) {
    should_record_metrics_ = false;
    return;
  }

  // TODO(csharrison): Optimize the domain lookup.
  // Note: If either |url| or |first_party_url| is empty, SameDomainOrHost will
  // return false, and function execution will continue because it is considered
  // 3rd party. Since |first_party_url| is actually the |site_for_cookies|, this
  // will happen e.g. for a 3rd party iframe on document.cookie access.
  if (!url.is_valid() ||
      net::registry_controlled_domains::SameDomainOrHost(
          url, first_party_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return;
  }

  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // |registrable_domain| can be empty e.g. if |url| is on an IP address, or the
  // domain is itself a TLD, or it's a file URL (in which case it has no host),
  // etc. See comment for GetDomainAndRegistry() in
  // //net/base/registry_controlled_domains/registry_controlled_domains.h.
  if (registrable_domain.empty()) {
    if (url.has_host()) {
      registrable_domain = url.host();
    } else {
      return;
    }
  }

  auto it = third_party_cookie_access_types_.find(registrable_domain);

  if (it != third_party_cookie_access_types_.end()) {
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
  if (third_party_cookie_access_types_.size() >= 1000)
    return;

  third_party_cookie_access_types_.emplace(registrable_domain, access_type);
}

void ThirdPartyMetricsObserver::RecordMetrics() {
  if (!should_record_metrics_)
    return;

  int cookie_origin_reads = 0;
  int cookie_origin_writes = 0;
  int local_storage_origin_access = 0;
  int session_storage_origin_access = 0;

  for (auto it : third_party_cookie_access_types_) {
    cookie_origin_reads += it.second.read;
    cookie_origin_writes += it.second.write;
  }

  for (auto it : third_party_storage_access_types_) {
    local_storage_origin_access += it.second.local_storage;
    session_storage_origin_access += it.second.session_storage;
  }

  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.CookieRead",
                            cookie_origin_reads);
  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.CookieWrite",
                            cookie_origin_writes);
  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess",
      local_storage_origin_access);
  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess",
      session_storage_origin_access);
}
