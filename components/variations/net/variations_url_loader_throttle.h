// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_

#include <memory>
#include <vector>

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace variations {

class VariationsClient;

// For non-incognito sessions, this class is created per request. If the
// requests is for a google domains, it adds variations where appropriate (see
// VariationsHeaderHelper::AppendHeaderIfNeeded) and removes them on redirect
// if necessary.
class VariationsURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<VariationsURLLoaderThrottle> {
 public:
  VariationsURLLoaderThrottle(const std::string& variation_ids_header);
  ~VariationsURLLoaderThrottle() override;

  VariationsURLLoaderThrottle(VariationsURLLoaderThrottle&&) = delete;
  VariationsURLLoaderThrottle(const VariationsURLLoaderThrottle&) = delete;

  VariationsURLLoaderThrottle& operator==(VariationsURLLoaderThrottle&&) =
      delete;
  VariationsURLLoaderThrottle& operator==(const VariationsURLLoaderThrottle&) =
      delete;

  // If |variations_client| isn't null and the user isn't incognito then a
  // throttle will be appended.
  static void AppendThrottleIfNeeded(
      const variations::VariationsClient* variations_client,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles);

 private:
  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* to_be_removed_headers,
                           net::HttpRequestHeaders* modified_headers) override;

  const std::string variation_ids_header_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_
