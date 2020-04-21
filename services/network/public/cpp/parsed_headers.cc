// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
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

  // TODO(arthursonzogni): Parse COOP and COEP here. Something like:
  //
  // ParseCrossOriginEmbedderPolicy(headers);
  // ParseCrossOriginOpenerPolicy(headers)
  return parsed_headers;
}

}  // namespace network
