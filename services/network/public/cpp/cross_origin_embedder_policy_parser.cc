// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"

#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {
constexpr char kHeaderName[] = "cross-origin-embedder-policy";
constexpr char kReportOnlyHeaderName[] =
    "cross-origin-embedder-policy-report-only";
}  // namespace

COMPONENT_EXPORT(NETWORK_CPP)
CrossOriginEmbedderPolicy ParseCrossOriginEmbedderPolicy(
    const net::HttpResponseHeaders& headers) {
  CrossOriginEmbedderPolicy coep;
  if (!base::FeatureList::IsEnabled(features::kCrossOriginEmbedderPolicy))
    return coep;

  std::string header_value;
  if (headers.GetNormalizedHeader(kHeaderName, &header_value)) {
    std::tie(coep.value, coep.reporting_endpoint) =
        network::CrossOriginEmbedderPolicy::Parse(header_value);
  }
  if (headers.GetNormalizedHeader(kReportOnlyHeaderName, &header_value)) {
    std::tie(coep.report_only_value, coep.report_only_reporting_endpoint) =
        network::CrossOriginEmbedderPolicy::Parse(header_value);
  }
  return coep;
}

}  // namespace network
