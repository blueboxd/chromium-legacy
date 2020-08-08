// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace prerender {
class PrerenderManager;
}

namespace weblayer {

// Singleton that owns all PrerenderManagers and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated PrerenderManager.
class PrerenderManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PrerenderManager for |context|.
  static prerender::PrerenderManager* GetForBrowserContext(
      content::BrowserContext* context);

  static PrerenderManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrerenderManagerFactory>;

  PrerenderManagerFactory();
  ~PrerenderManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_FACTORY_H_
