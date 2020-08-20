// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_TEST_AFFILIATION_FETCHER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_TEST_AFFILIATION_FETCHER_FACTORY_H_

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace password_manager {

class AffiliationFetcherInterface;
class AffiliationFetcherDelegate;

// Interface for a factory to be used by AffiliationFetcher::Create() in tests
// to construct instances of test-specific AffiliationFetcher subclasses.
//
// The factory is registered with AffiliationFetcher::SetFactoryForTesting().
class TestAffiliationFetcherFactory {
 public:
  TestAffiliationFetcherFactory(const TestAffiliationFetcherFactory&) = delete;
  TestAffiliationFetcherFactory& operator=(
      const TestAffiliationFetcherFactory&) = delete;

  // Constructs a fetcher to retrieve affiliations for each facet in |facet_ids|
  // using the specified |url_loader_factory|, and will provide the results
  // to the |delegate| on the same thread that creates the instance.
  virtual AffiliationFetcherInterface* CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate) = 0;

 protected:
  TestAffiliationFetcherFactory() = default;
  virtual ~TestAffiliationFetcherFactory() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_TEST_AFFILIATION_FETCHER_FACTORY_H_
