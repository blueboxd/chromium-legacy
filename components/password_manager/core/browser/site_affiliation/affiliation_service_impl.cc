// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/sync/driver/sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace password_manager {

AffiliationServiceImpl::AffiliationServiceImpl(
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : sync_service_(sync_service),
      url_loader_factory_(std::move(url_loader_factory)) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::PrefetchChangePasswordURLs(
    const std::vector<url::SchemeHostPort>& tuple_origins) {
  if (ShouldAffiliationBasedMatchingBeActive(sync_service_)) {
    RequestFacetsAffiliations(
        ConvertMissingSchemeHostPortsToFacets(tuple_origins));
  }
}

void AffiliationServiceImpl::Clear() {
  fetcher_.reset();
  change_password_urls_.clear();
}

GURL AffiliationServiceImpl::GetChangePasswordURL(
    const url::SchemeHostPort& tuple) const {
  auto it = change_password_urls_.find(tuple);
  return it != change_password_urls_.end() ? it->second : GURL();
}

void AffiliationServiceImpl::OnFetchSucceeded(
    std::unique_ptr<AffiliationFetcherDelegate::Result> result) {}

void AffiliationServiceImpl::OnFetchFailed() {}

void AffiliationServiceImpl::OnMalformedResponse() {}

std::vector<FacetURI>
AffiliationServiceImpl::ConvertMissingSchemeHostPortsToFacets(
    const std::vector<url::SchemeHostPort>& tuple_origins) {
  std::vector<FacetURI> facets;
  for (const auto& tuple : tuple_origins) {
    if (tuple.IsValid() && !base::Contains(change_password_urls_, tuple)) {
      requested_tuple_origins_.push_back(tuple);
      facets.push_back(FacetURI::FromCanonicalSpec(tuple.Serialize()));
    }
  }
  return facets;
}

// TODO(crbug.com/1117045): New request resets the pointer to
// AffiliationFetcher, therefore the previous request gets canceled.
void AffiliationServiceImpl::RequestFacetsAffiliations(
    const std::vector<FacetURI>& facets) {
  fetcher_.reset(AffiliationFetcher::Create(url_loader_factory_, this));
  fetcher_->StartRequest(facets);
}

}  // namespace password_manager
