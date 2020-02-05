// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_HEADER_UTIL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_HEADER_UTIL_H_

#include "content/public/common/previews_state.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

// Adds a previews-specific directive to the Chrome-Proxy-Accept-Transform
// header if needed.
void MaybeSetAcceptTransformHeader(const GURL& url,
                                   blink::mojom::ResourceType resource_type,
                                   content::PreviewsState previews_state,
                                   net::HttpRequestHeaders* headers);

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_HEADER_UTIL_H_
