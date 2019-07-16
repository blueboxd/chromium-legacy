// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator.h"

#include "base/time/time.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"

namespace content {

NavigatorDelegate* Navigator::GetDelegate() {
  return nullptr;
}

NavigationController* Navigator::GetController() {
  return nullptr;
}

bool Navigator::StartHistoryNavigationInNewSubframe(
    RenderFrameHostImpl* render_frame_host,
    mojom::NavigationClientAssociatedPtrInfo* navigation_client) {
  return false;
}

base::TimeTicks Navigator::GetCurrentLoadStart() {
  return base::TimeTicks::Now();
}

void Navigator::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache) {}

}  // namespace content
