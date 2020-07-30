// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

namespace blink {

bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (!headers)
    return false;

  // RFC 2616, section 14.9.
  return headers->HasHeaderValue("cache-control", "no-cache") ||
         headers->HasHeaderValue("cache-control", "no-store") ||
         headers->HasHeaderValue("pragma", "no-cache") ||
         headers->HasHeaderValue("vary", "*");
}

network::mojom::ReferrerPolicy NetToMojoReferrerPolicy(
    net::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case net::ReferrerPolicy::NEVER_CLEAR:
      return network::mojom::ReferrerPolicy::kAlways;
    case net::ReferrerPolicy::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case net::ReferrerPolicy::
        ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case net::ReferrerPolicy::NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

}  // namespace blink
