// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_throttles.h"

#include "components/variations/net/omnibox_url_loader_throttle.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          request, browser_context, wc_getter, navigation_ui_data,
          frame_tree_node_id);
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(&throttles);
  variations::VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
      browser_context->GetVariationsClient(), &throttles);
  return throttles;
}

}  // namespace content
