// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"

#include "chrome/browser/prerender/isolated/isolated_prerender_proxying_url_loader_factory.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

IsolatedPrerenderSubresourceManager::IsolatedPrerenderSubresourceManager(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response)
    : url_(url), mainframe_response_(std::move(mainframe_response)) {}

IsolatedPrerenderSubresourceManager::~IsolatedPrerenderSubresourceManager() {
  if (nsp_handle_) {
    nsp_handle_->OnCancel();
    nsp_handle_->SetObserver(nullptr);
  }
}

void IsolatedPrerenderSubresourceManager::ManageNoStatePrefetch(
    std::unique_ptr<prerender::PrerenderHandle> handle,
    base::OnceClosure on_nsp_done_callback) {
  on_nsp_done_callback_ = std::move(on_nsp_done_callback);
  nsp_handle_ = std::move(handle);
  nsp_handle_->SetObserver(this);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderSubresourceManager::TakeMainframeResponse() {
  return std::move(mainframe_response_);
}

void IsolatedPrerenderSubresourceManager::SetIsolatedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> isolated_loader_factory) {
  isolated_loader_factory_ = isolated_loader_factory;
}

void IsolatedPrerenderSubresourceManager::OnPrerenderStop(
    prerender::PrerenderHandle* handle) {
  DCHECK_EQ(nsp_handle_.get(), handle);

  if (on_nsp_done_callback_) {
    std::move(on_nsp_done_callback_).Run();
  }

  // The handle must be canceled before it can be destroyed.
  nsp_handle_->OnCancel();
  nsp_handle_.reset();
}

bool IsolatedPrerenderSubresourceManager::MaybeProxyURLLoaderFactory(
    int render_process_id,
    int frame_tree_node_id,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver) {
  if (type != content::ContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource) {
    return false;
  }

  if (!nsp_handle_) {
    return false;
  }

  content::WebContents* web_contents =
      nsp_handle_->contents()->prerender_contents();
  if (!web_contents) {
    // This shouldn't happen, so abort the prerender just to be safe.
    OnPrerenderStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  int prerender_process_id =
      web_contents->GetMainFrame()->GetProcess()->GetID();
  if (prerender_process_id != render_process_id) {
    return false;
  }

  if (!isolated_loader_factory_) {
    // This also shouldn't happen, and would imply that there is a bug in the
    // code where a prerender was triggered without having an ioslated URL
    // Loader Factory to use. Abort the prerender just to be safe.
    OnPrerenderStop(nsp_handle_.get());
    NOTREACHED();
    return false;
  }

  auto proxied_receiver = std::move(*factory_receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      network_process_factory_remote;
  *factory_receiver =
      network_process_factory_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory_remote;
  isolated_loader_factory_->Clone(
      isolated_factory_remote.InitWithNewPipeAndPassReceiver());

  auto proxy = std::make_unique<IsolatedPrerenderProxyingURLLoaderFactory>(
      frame_tree_node_id, std::move(proxied_receiver),
      std::move(network_process_factory_remote),
      std::move(isolated_factory_remote),
      base::BindOnce(
          &IsolatedPrerenderSubresourceManager::RemoveProxiedURLLoaderFactory,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &IsolatedPrerenderSubresourceManager::OnSubresourceLoadSuccessful,
          weak_factory_.GetWeakPtr()));
  proxied_loader_factories_.emplace(std::move(proxy));

  return true;
}

void IsolatedPrerenderSubresourceManager::OnSubresourceLoadSuccessful(
    const GURL& url) {
  successfully_loaded_subresources_.emplace(url);
}

void IsolatedPrerenderSubresourceManager::RemoveProxiedURLLoaderFactory(
    IsolatedPrerenderProxyingURLLoaderFactory* factory) {
  auto it = proxied_loader_factories_.find(factory);
  DCHECK(it != proxied_loader_factories_.end());
  proxied_loader_factories_.erase(it);
}
