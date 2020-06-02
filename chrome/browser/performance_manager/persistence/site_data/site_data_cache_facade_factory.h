// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace performance_manager {

class SiteDataCacheFacade;
class SiteDataCacheFactory;
class SiteDataCacheFacadeTest;

// BrowserContextKeyedServiceFactory that adorns each browser context with a
// SiteDataCacheFacade.
//
// There's several components to the SiteDataCache architecture:
//   - SiteDataCacheFacade: A KeyedService living on the UI thread that is used
//     as a facade for a SiteDataCache object living on a separate sequence.
//     There's one instance of this class per profile.
//   - SiteDataCacheFacadeFactory: A KeyedService factory living on the UI
//     thread that adorns each profile with a SiteDataCacheFacade. A counterpart
//     to this class, SiteDataCacheFactory, lives on the same sequence as the
//     SiteDataCache objects to manage their lifetime.
//
// The lifetime of these objects is the following:
//   - At startup, the SiteDataCacheFacadeFactory singleton gets initialized on
//     the UI thread. It creates its SiteDataCacheFactory counterpart living on
//     a separate sequence and wraps it in a base::SequenceBound object to
//     ensure that it only gets used from the appropriate sequence.
//   - When a browser context is created, the SiteDataCacheFacadeFactory object
//     produces a SiteDataCacheFacade for the profile. The creation of this
//     facade causes the creation of a SiteDataCache on the sequence that uses
//     these objects.
//   - When a browser context is destroyed the corresponding SiteDataCacheFacade
//     is destroyed and this also destroys the corresponding SiteDataCache on
//     the proper sequence (via the SequenceBound object).
//   - At shutdown, the SiteDataCacheFacadeFactory is destroyed shortly before
//     terminating the thread pool. Destruction of this object causes the
//     SiteDataCacheFactory to be destroyed on its sequence.
class SiteDataCacheFacadeFactory : public BrowserContextKeyedServiceFactory {
 public:
  ~SiteDataCacheFacadeFactory() override;

  static SiteDataCacheFacade* GetForProfile(Profile* profile);
  static SiteDataCacheFacadeFactory* GetInstance();

 protected:
  friend class base::NoDestructor<SiteDataCacheFacadeFactory>;
  friend class SiteDataCacheFacade;
  friend class SiteDataCacheFacadeTest;

  SiteDataCacheFacadeFactory();

  base::SequenceBound<SiteDataCacheFactory>* cache_factory() {
    return &cache_factory_;
  }

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  // The counterpart of this factory living on the SiteDataCache's sequence.
  base::SequenceBound<SiteDataCacheFactory> cache_factory_;

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheFacadeFactory);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
