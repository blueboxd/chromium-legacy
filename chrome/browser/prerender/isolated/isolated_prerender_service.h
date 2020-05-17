// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;
class IsolatedPrerenderProxyConfigurator;
class IsolatedPrerenderServiceWorkersObserver;
class PrefetchedMainframeResponseContainer;

// This service owns browser-level objects used in Isolated Prerenders.
class IsolatedPrerenderService
    : public KeyedService,
      public data_reduction_proxy::DataReductionProxySettingsObserver {
 public:
  explicit IsolatedPrerenderService(Profile* profile);
  ~IsolatedPrerenderService() override;

  IsolatedPrerenderProxyConfigurator* proxy_configurator() {
    return proxy_configurator_.get();
  }

  IsolatedPrerenderServiceWorkersObserver* service_workers_observer() {
    return service_workers_observer_.get();
  }

  void OnAboutToNoStatePrefetch(
      const GURL& url,
      std::unique_ptr<PrefetchedMainframeResponseContainer> response);

  std::unique_ptr<PrefetchedMainframeResponseContainer>
  TakeResponseForNoStatePrefetch(const GURL& url);

  IsolatedPrerenderService(const IsolatedPrerenderService&) = delete;
  IsolatedPrerenderService& operator=(const IsolatedPrerenderService&) = delete;

 private:
  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnProxyRequestHeadersChanged(
      const net::HttpRequestHeaders& headers) override;
  void OnSettingsInitialized() override;
  void OnDataSaverEnabledChanged(bool enabled) override;
  void OnPrefetchProxyHostsChanged(
      const std::vector<GURL>& prefetch_proxies) override;

  // KeyedService:
  void Shutdown() override;

  // Cleans up the NoStatePrerender response. Used in a delayed post task.
  void CleanupNoStatePrefetchResponse(const GURL& url);

  // The current profile, not owned.
  Profile* profile_;

  // The custom proxy configurator for Isolated Prerenders.
  std::unique_ptr<IsolatedPrerenderProxyConfigurator> proxy_configurator_;

  // The storage partition-level observer of registered service workers.
  std::unique_ptr<IsolatedPrerenderServiceWorkersObserver>
      service_workers_observer_;

  // The cached mainframe responses that will be used in an upcoming
  // NoStatePrefetch. Kept at the browser level because the NSP happens in a
  // different WebContents than the one that initiated it.
  std::map<GURL, std::unique_ptr<PrefetchedMainframeResponseContainer>>
      no_state_prefetch_responses_;

  base::WeakPtrFactory<IsolatedPrerenderService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_
