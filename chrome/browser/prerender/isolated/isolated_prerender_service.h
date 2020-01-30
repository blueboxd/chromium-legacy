// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_

#include <memory>

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class IsolatedPrerenderProxyConfigurator;

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

 private:
  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnProxyRequestHeadersChanged(
      const net::HttpRequestHeaders& headers) override;
  void OnSettingsInitialized() override;
  void OnDataSaverEnabledChanged(bool enabled) override;

  // The current profile, not owned.
  Profile* profile_;

  // The custom proxy configurator for Isolated Prerenders.
  std::unique_ptr<IsolatedPrerenderProxyConfigurator> proxy_configurator_;

  IsolatedPrerenderService(const IsolatedPrerenderService&) = delete;
  IsolatedPrerenderService& operator=(const IsolatedPrerenderService&) = delete;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_
