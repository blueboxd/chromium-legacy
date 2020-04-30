// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

// Const definition of the strings involved in the header parsing.
constexpr char kCrossOriginOpenerPolicyHeader[] = "Cross-Origin-Opener-Policy";
const char kSameOrigin[] = "same-origin";
const char kSameOriginAllowPopups[] = "same-origin-allow-popups";
}  // namespace

mojom::CrossOriginOpenerPolicy ParseCrossOriginOpenerPolicy(
    const net::HttpResponseHeaders& headers) {
  std::string header_value;
  if (!base::FeatureList::IsEnabled(features::kCrossOriginOpenerPolicy))
    return mojom::CrossOriginOpenerPolicy::kUnsafeNone;

  if (!headers.GetNormalizedHeader(kCrossOriginOpenerPolicyHeader,
                                   &header_value)) {
    return mojom::CrossOriginOpenerPolicy::kUnsafeNone;
  }

  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (item && item->item.Type() == Item::kTokenType) {
    const auto& policy_item = item->item.GetString();
    if (policy_item == kSameOrigin)
      return mojom::CrossOriginOpenerPolicy::kSameOrigin;
    if (policy_item == kSameOriginAllowPopups)
      return mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups;
  }

  // Default to kUnsafeNone for all malformed values and "unsafe-none"
  return mojom::CrossOriginOpenerPolicy::kUnsafeNone;
}

}  // namespace network
