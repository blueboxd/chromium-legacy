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
#include "content/public/browser/content_browser_client.h"
#include "url/gurl.h"

class Profile;
class IsolatedPrerenderProxyConfigurator;
class IsolatedPrerenderSubresourceManager;
class PrefetchedMainframeResponseContainer;

namespace content {
class RenderFrameHost;
}

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

  // This call is forwarded to all |IsolatedPrerenderSubresourceManager| in
  // |subresource_managers_| see documentation there for more detail.
  bool MaybeProxyURLLoaderFactory(
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          factory_receiver);

  // Creates an |IsolatedPrerenderSubresourceManager| for the given |url|.
  IsolatedPrerenderSubresourceManager* OnAboutToNoStatePrefetch(
      const GURL& url,
      std::unique_ptr<PrefetchedMainframeResponseContainer> response);

  // Returns a pointer to an |IsolatedPrerenderSubresourceManager| for the given
  // URL, if one exists and hasn't been destroyed. Do not hold on to the
  // returned pointer since it may be deleted without notice.
  IsolatedPrerenderSubresourceManager* GetSubresourceManagerForURL(
      const GURL& url) const;

  // Passes ownership of an |IsolatedPrerenderSubresourceManager| for the given
  // URL, if one exists and hasn't been destroyed.
  std::unique_ptr<IsolatedPrerenderSubresourceManager>
  TakeSubresourceManagerForURL(const GURL& url);

  // Destroys the subresource manager for the given url if one exists.
  void DestroySubresourceManagerForURL(const GURL& url);

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

  // Map of prerender URL to its manager. Kept at the browser level since NSPs
  // are done in a separate WebContents from the one they are created in.
  std::map<GURL, std::unique_ptr<IsolatedPrerenderSubresourceManager>>
      subresource_managers_;

  base::WeakPtrFactory<IsolatedPrerenderService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SERVICE_H_
