// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/initiator_lock_compatibility.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

namespace {

bool IsSameSite(const url::Origin& initiator,
                const url::Origin& target_origin) {
  // Cross-scheme initiator should be considered cross-site (even if it's host
  // is same-site with the target).  See also https://crbug.com/979257.
  if (initiator.scheme() != target_origin.scheme())
    return false;

  return net::registry_controlled_domains::SameDomainOrHost(
      initiator, target_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Possible values of Sec-Fetch-Site header.
//
// Note that the order of enum values below is significant - it is important for
// std::max invocations that kSameOrigin < kSameSite < kCrossSite.
enum class HeaderValue {
  kNoOrigin,
  kSameOrigin,
  kSameSite,
  kCrossSite,
};

HeaderValue CalculateHeaderValue(const GURL& target_url,
                                 const url::Origin& initiator) {
  url::Origin target_origin = url::Origin::Create(target_url);

  if (target_origin == initiator)
    return HeaderValue::kSameOrigin;

  if (IsSameSite(initiator, target_origin))
    return HeaderValue::kSameSite;

  return HeaderValue::kCrossSite;
}

HeaderValue CalculateHeaderValue(
    const net::URLRequest& request,
    const GURL* pending_redirect_url,
    const mojom::URLLoaderFactoryParams& factory_params) {
  // Use `Sec-Fetch-Site: none` for browser-initiated requests with no
  // initiator origin.
  if (factory_params.process_id == mojom::kBrowserProcessId &&
      !request.initiator().has_value()) {
    return HeaderValue::kNoOrigin;
  }

  // Otherwise, calculate the |header_value| by comparing the initiator with the
  // full chain of request URLs.
  HeaderValue header_value = HeaderValue::kSameOrigin;
  url::Origin initiator = GetTrustworthyInitiator(
      factory_params.request_initiator_site_lock, request.initiator());
  for (const GURL& target_url : request.url_chain()) {
    header_value =
        std::max(header_value, CalculateHeaderValue(target_url, initiator));
  }
  if (pending_redirect_url) {
    header_value = std::max(
        header_value, CalculateHeaderValue(*pending_redirect_url, initiator));
  }
  return header_value;
}

const char* GetHeaderString(const HeaderValue& value) {
  switch (value) {
    case HeaderValue::kNoOrigin:
      return "none";
    case HeaderValue::kSameOrigin:
      return "same-origin";
    case HeaderValue::kSameSite:
      return "same-site";
    case HeaderValue::kCrossSite:
      return "cross-site";
  }
}

}  // namespace

void SetSecFetchSiteHeader(
    net::URLRequest* request,
    const GURL* pending_redirect_url,
    const mojom::URLLoaderFactoryParams& factory_params) {
  DCHECK(request);
  DCHECK_NE(0u, request->url_chain().size());
  if (!base::FeatureList::IsEnabled(features::kFetchMetadata))
    return;

  // Only append the header to potentially trustworthy URLs.
  const GURL& target_url =
      pending_redirect_url ? *pending_redirect_url : request->url();
  if (!IsUrlPotentiallyTrustworthy(target_url))
    return;

  // Set the request header.
  const char kHeaderName[] = "Sec-Fetch-Site";
  HeaderValue header_value =
      CalculateHeaderValue(*request, pending_redirect_url, factory_params);
  request->SetExtraRequestHeaderByName(
      kHeaderName, GetHeaderString(header_value), /* overwrite = */ true);
}

void MaybeRemoveSecHeaders(net::URLRequest* request,
                           const GURL& pending_redirect_url) {
  DCHECK(request);

  if (!base::FeatureList::IsEnabled(features::kFetchMetadata))
    return;

  // If our redirect destination is not trusted it would not have had sec-ch- or
  // sec-fetch- prefixed headers added to it. Our previous hops may have added
  // these headers if the current url is trustworthy though so we should try to
  // remove these now.
  if (IsUrlPotentiallyTrustworthy(request->url()) &&
      !IsUrlPotentiallyTrustworthy(pending_redirect_url)) {
    // Check each of our request headers and if it is a "sec-ch-" or
    // "sec-fetch-" prefixed header we'll remove it.
    const net::HttpRequestHeaders::HeaderVector request_headers =
        request->extra_request_headers().GetHeaderVector();
    for (const auto& header : request_headers) {
      if (StartsWith(header.key, "sec-ch-",
                     base::CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(header.key, "sec-fetch-",
                     base::CompareCase::INSENSITIVE_ASCII)) {
        request->RemoveRequestHeaderByName(header.key);
      }
    }
  }
}

}  // namespace network
