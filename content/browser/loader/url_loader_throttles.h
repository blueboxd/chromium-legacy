// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_THROTTLES_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_THROTTLES_H_

#include "base/callback.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class BrowserContext;
class NavigationUIData;
class WebContents;

// Wrapper around ContentBrowserClient::CreateURLLoaderThrottles which inserts
// additional content specific throttles.
std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id);

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_THROTTLES_H_
