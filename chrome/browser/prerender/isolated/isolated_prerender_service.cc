// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/profiles/profile.h"

IsolatedPrerenderService::IsolatedPrerenderService(Profile* profile)
    : profile_(profile),
      proxy_configurator_(
          std::make_unique<IsolatedPrerenderProxyConfigurator>()) {
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
  if (drp_settings)
    drp_settings->AddDataReductionProxySettingsObserver(this);
}

IsolatedPrerenderService::~IsolatedPrerenderService() {
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
  if (drp_settings)
    drp_settings->RemoveDataReductionProxySettingsObserver(this);
}

void IsolatedPrerenderService::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  proxy_configurator_->UpdateTunnelHeaders(headers);
}

void IsolatedPrerenderService::OnSettingsInitialized() {}
void IsolatedPrerenderService::OnDataSaverEnabledChanged(bool enabled) {}
