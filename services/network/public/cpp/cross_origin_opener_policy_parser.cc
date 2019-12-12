// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace network {

namespace {

// Const definition of the strings involved in the header parsing.
const char kSameOrigin[] = "same-origin";
const char kSameOriginAllowPopups[] = "same-origin-allow-popups";

// Spec's HTTP tab or space: https://fetch.spec.whatwg.org/#http-tab-or-space.
const char kHTTPTabOrSpace[] = {0x09, /* CHARACTER TABULATION */
                                0x20, /* SPACE */
                                0};

}  // namespace

mojom::CrossOriginOpenerPolicy ParseCrossOriginOpenerPolicyHeader(
    const std::string& raw_coop_string) {
  base::StringPiece trimmed_value =
      base::TrimString(raw_coop_string, kHTTPTabOrSpace, base::TRIM_ALL);
  if (trimmed_value == kSameOrigin)
    return mojom::CrossOriginOpenerPolicy::kSameOrigin;
  if (trimmed_value == kSameOriginAllowPopups)
    return mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups;
  // Default to kUnsafeNone for all malformed values and "unsafe-none"
  return mojom::CrossOriginOpenerPolicy::kUnsafeNone;
}

}  // namespace network
