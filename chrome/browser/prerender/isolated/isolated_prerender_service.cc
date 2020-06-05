// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"

#include "base/bind.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"
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

bool IsolatedPrerenderService::MaybeProxyURLLoaderFactory(
    int render_process_id,
    int frame_tree_node_id,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver) {
  for (const auto& manager_pair : subresource_managers_) {
    if (manager_pair.second->MaybeProxyURLLoaderFactory(
            render_process_id, frame_tree_node_id, type, factory_receiver)) {
      return true;
    }
  }
  return false;
}

IsolatedPrerenderSubresourceManager*
IsolatedPrerenderService::OnAboutToNoStatePrefetch(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> response) {
  std::unique_ptr<IsolatedPrerenderSubresourceManager> manager =
      std::make_unique<IsolatedPrerenderSubresourceManager>(
          url, std::move(response));
  IsolatedPrerenderSubresourceManager* manager_ptr = manager.get();
  subresource_managers_.emplace(url, std::move(manager));
  return manager_ptr;
}

IsolatedPrerenderSubresourceManager*
IsolatedPrerenderService::GetSubresourceManagerForURL(const GURL& url) const {
  auto iter = subresource_managers_.find(url);
  if (iter == subresource_managers_.end())
    return nullptr;
  return iter->second.get();
}

void IsolatedPrerenderService::DestroySubresourceManagerForURL(
    const GURL& url) {
  auto iter = subresource_managers_.find(url);
  if (iter != subresource_managers_.end()) {
    subresource_managers_.erase(iter);
  }
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
