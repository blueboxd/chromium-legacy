// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace prerender {
class PrerenderHandle;
}

class PrefetchedMainframeResponseContainer;
class IsolatedPrerenderProxyingURLLoaderFactory;

// This class manages the isolated prerender of a page and its subresources.
class IsolatedPrerenderSubresourceManager
    : public prerender::PrerenderHandle::Observer {
 public:
  explicit IsolatedPrerenderSubresourceManager(
      const GURL& url,
      std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response);
  ~IsolatedPrerenderSubresourceManager() override;

  // Passes ownership of |handle| to |this|, calling |on_nsp_done_callback| when
  // the NSP is done.
  void ManageNoStatePrefetch(std::unique_ptr<prerender::PrerenderHandle> handle,
                             base::OnceClosure on_nsp_done_callback);

  bool has_nsp_handle() const { return !!nsp_handle_; }

  const std::set<GURL>& successfully_loaded_subresources() const {
    return successfully_loaded_subresources_;
  }

  // Takes ownership of |mainframe_response_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakeMainframeResponse();

  // Gives |this| a reference to the isolated URL Loader factory to use for
  // Isolated Prerenders.
  void SetIsolatedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> isolated_loader_factory);

  // Called on commit to allow |this| to setup an intermediary (AKA: proxy, not
  // to be confused with a proxy server) URLLoaderFactory between the renderer
  // and network stack. Returns true when |factory_receiver| is proxied.
  bool MaybeProxyURLLoaderFactory(
      int render_process_id,
      int frame_tree_node_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          factory_receiver);

  // prerender::PrerenderHandle::Observer:
  void OnPrerenderStart(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStopLoading(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderDomContentLoaded(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override;
  void OnPrerenderNetworkBytesChanged(
      prerender::PrerenderHandle* handle) override {}

  IsolatedPrerenderSubresourceManager(
      const IsolatedPrerenderSubresourceManager&) = delete;
  IsolatedPrerenderSubresourceManager& operator=(
      const IsolatedPrerenderSubresourceManager&) = delete;

 private:
  // Called when the given factory is done serving all requests and can be
  // destroyed.
  void RemoveProxiedURLLoaderFactory(
      IsolatedPrerenderProxyingURLLoaderFactory* factory);

  // Used as
  // |IsolatedPrerenderProxyingURLLoaderFactory::ResourceLoadSuccessfulCallback|
  // to report a subresource was successfully loaded during the NSP.
  void OnSubresourceLoadSuccessful(const GURL& url);

  // The page that is being prerendered.
  const GURL url_;

  // All the subresources that have been successfully loaded during the NSP.
  // Each step in a subresource's redirect chain is also added here so that
  // those steps can be used from the network cache later as well.
  std::set<GURL> successfully_loaded_subresources_;

  // The mainframe response headers and body.
  std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response_;

  // State for managing the NoStatePrerender when it is running. If
  // |nsp_handle_| is set, then |on_nsp_done_callback_| is also set and vise
  // versa.
  std::unique_ptr<prerender::PrerenderHandle> nsp_handle_;
  base::OnceClosure on_nsp_done_callback_;

  // All owned proxying URL Loader Factories.
  std::set<std::unique_ptr<IsolatedPrerenderProxyingURLLoaderFactory>,
           base::UniquePtrComparator>
      proxied_loader_factories_;

  // The isolated URL Loader Factory (with proxy) to use during NSP.
  scoped_refptr<network::SharedURLLoaderFactory> isolated_loader_factory_;

  base::WeakPtrFactory<IsolatedPrerenderSubresourceManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_
