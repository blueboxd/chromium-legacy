// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

IsolatedPrerenderService::IsolatedPrerenderService(Profile* profile)
    : profile_(profile),
      proxy_configurator_(
          std::make_unique<IsolatedPrerenderProxyConfigurator>()),
      service_workers_observer_(
          std::make_unique<IsolatedPrerenderServiceWorkersObserver>(profile)) {
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
  if (drp_settings)
    drp_settings->AddDataReductionProxySettingsObserver(this);
}

IsolatedPrerenderService::~IsolatedPrerenderService() = default;

void IsolatedPrerenderService::Shutdown() {
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
  if (drp_settings)
    drp_settings->RemoveDataReductionProxySettingsObserver(this);
}

void IsolatedPrerenderService::OnAboutToNoStatePrefetch(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> response) {
  no_state_prefetch_responses_.emplace(url, std::move(response));

  // Schedule a cleanup in just a short time so that any edge case that causes a
  // response not to be used (like the user navigating away inside of a narrow
  // window between the response being copied here and taken) doesn't cause a
  // memory leak.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&IsolatedPrerenderService::CleanupNoStatePrefetchResponse,
                     weak_factory_.GetWeakPtr(), url),
      // 30s is ample time since the mainframe can always be anonymously
      // re-fetched if the NSP fails to start the renderer in this time.
      base::TimeDelta::FromSeconds(30));
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderService::TakeResponseForNoStatePrefetch(const GURL& url) {
  auto iter = no_state_prefetch_responses_.find(url);
  if (iter == no_state_prefetch_responses_.end()) {
    return nullptr;
  }

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      std::move(iter->second);
  no_state_prefetch_responses_.erase(iter);

  return resp;
}

void IsolatedPrerenderService::CleanupNoStatePrefetchResponse(const GURL& url) {
  TakeResponseForNoStatePrefetch(url);
}

void IsolatedPrerenderService::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  proxy_configurator_->UpdateTunnelHeaders(headers);
}

void IsolatedPrerenderService::OnPrefetchProxyHostsChanged(
    const std::vector<GURL>& prefetch_proxies) {
  proxy_configurator_->UpdateProxyHosts(prefetch_proxies);
}

void IsolatedPrerenderService::OnSettingsInitialized() {}
void IsolatedPrerenderService::OnDataSaverEnabledChanged(bool enabled) {}
