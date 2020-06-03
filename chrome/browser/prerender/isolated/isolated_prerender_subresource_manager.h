// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
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

  // Takes ownership of |mainframe_response_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> TakeMainframeResponse();

  // Gives |this| a reference to the isolated URL Loader factory to use for
  // Isolated Prerenders.
  void SetIsolatedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> isolated_loader_factory);

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
  // The page that is being prerendered.
  const GURL url_;

  // The mainframe response headers and body.
  std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response_;

  // State for managing the NoStatePrerender when it is running. If
  // |nsp_handle_| is set, then |on_nsp_done_callback_| is also set and vise
  // versa.
  std::unique_ptr<prerender::PrerenderHandle> nsp_handle_;
  base::OnceClosure on_nsp_done_callback_;

  // The isolated URL Loader Factory (with proxy) to use during NSP.
  scoped_refptr<network::SharedURLLoaderFactory> isolated_loader_factory_;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_SUBRESOURCE_MANAGER_H_
