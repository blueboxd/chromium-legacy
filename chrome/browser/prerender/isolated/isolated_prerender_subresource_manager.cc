// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"

#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

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
