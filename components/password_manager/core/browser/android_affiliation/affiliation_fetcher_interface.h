// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_

#include <vector>

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

namespace password_manager {

class AffiliationFetcherInterface {
 public:
  AffiliationFetcherInterface() = default;
  virtual ~AffiliationFetcherInterface() = default;

  // Not copyable or movable
  AffiliationFetcherInterface(const AffiliationFetcherInterface&) = delete;
  AffiliationFetcherInterface& operator=(const AffiliationFetcherInterface&) =
      delete;
  AffiliationFetcherInterface(AffiliationFetcherInterface&&) = delete;
  AffiliationFetcherInterface& operator=(AffiliationFetcherInterface&&) =
      delete;

  // Starts the request to retrieve affiliations for each facet in
  // |facet_uris|.
  virtual void StartRequest(const std::vector<FacetURI>& facet_uris) = 0;

  // Returns requested facet uris.
  virtual const std::vector<FacetURI>& GetRequestedFacetURIs() const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_INTERFACE_H_
