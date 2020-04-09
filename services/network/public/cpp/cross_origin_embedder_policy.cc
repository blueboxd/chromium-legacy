// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy.h"

#include <algorithm>

#include "net/http/structured_headers.h"

namespace network {

namespace {
constexpr char kRequireCorp[] = "require-corp";
}  // namespace

const char CrossOriginEmbedderPolicy::kHeaderName[] =
    "cross-origin-embedder-policy";
const char CrossOriginEmbedderPolicy::kReportOnlyHeaderName[] =
    "cross-origin-embedder-policy-report-only";

CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy() = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    CrossOriginEmbedderPolicy&& src) = default;
CrossOriginEmbedderPolicy::~CrossOriginEmbedderPolicy() = default;

CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    CrossOriginEmbedderPolicy&& src) = default;
bool CrossOriginEmbedderPolicy::operator==(
    const CrossOriginEmbedderPolicy& other) const {
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint == other.report_only_reporting_endpoint;
}

std::pair<mojom::CrossOriginEmbedderPolicyValue, base::Optional<std::string>>
CrossOriginEmbedderPolicy::Parse(base::StringPiece header_value) {
  constexpr auto kNone = mojom::CrossOriginEmbedderPolicyValue::kNone;
  using Item = net::structured_headers::Item;
  const auto item = net::structured_headers::ParseItem(header_value);
  if (!item || item->item.Type() != Item::kTokenType ||
      item->item.GetString() != kRequireCorp) {
    return std::make_pair(kNone, base::nullopt);
  }
  base::Optional<std::string> endpoint;
  auto it = std::find_if(item->params.cbegin(), item->params.cend(),
                         [](const std::pair<std::string, Item>& param) {
                           return param.first == "report-to";
                         });
  if (it != item->params.end() && it->second.Type() == Item::kStringType) {
    endpoint = it->second.GetString();
  }
  return std::make_pair(mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
                        std::move(endpoint));
}

}  // namespace network
