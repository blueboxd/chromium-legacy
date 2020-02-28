// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_url_loader_throttle.h"

#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_http_header_provider.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace variations {

VariationsURLLoaderThrottle::VariationsURLLoaderThrottle(
    const std::string& variation_ids_header)
    : variation_ids_header_(variation_ids_header) {}

VariationsURLLoaderThrottle::~VariationsURLLoaderThrottle() = default;

// static
void VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
    const variations::VariationsClient* variations_client,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  if (!variations_client || variations_client->IsIncognito())
    return;

  throttles->push_back(std::make_unique<VariationsURLLoaderThrottle>(
      variations_client->GetVariationsHeader()));
}

void VariationsURLLoaderThrottle::DetachFromCurrentSequence() {}

void VariationsURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // This throttle is never created when incognito so we pass in
  // variations::InIncognito::kNo.
  variations::AppendVariationsHeaderWithCustomValue(
      request->url, variations::InIncognito::kNo, variation_ids_header_,
      request);
}

void VariationsURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers) {
  variations::RemoveVariationsHeaderIfNeeded(*redirect_info, response_head,
                                             to_be_removed_headers);
}

}  // namespace variations
