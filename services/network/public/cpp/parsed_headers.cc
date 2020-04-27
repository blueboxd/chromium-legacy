// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"
#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"
#include "services/network/public/cpp/features.h"

namespace network {

mojom::ParsedHeadersPtr PopulateParsedHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const GURL& url) {
  auto parsed_headers = mojom::ParsedHeaders::New();
  if (!headers)
    return parsed_headers;

  if (base::FeatureList::IsEnabled(features::kOutOfBlinkFrameAncestors)) {
    AddContentSecurityPolicyFromHeaders(
        *headers, url, &parsed_headers->content_security_policy);
  }

  parsed_headers->cross_origin_embedder_policy =
      ParseCrossOriginEmbedderPolicy(*headers);
  parsed_headers->cross_origin_opener_policy =
      ParseCrossOriginOpenerPolicy(*headers);

  std::string accept_ch;
  if (headers->GetNormalizedHeader("Accept-CH", &accept_ch))
    parsed_headers->accept_ch = ParseAcceptCH(accept_ch);

  return parsed_headers;
}

}  // namespace network
