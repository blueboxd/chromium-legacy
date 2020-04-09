// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-shared.h"

namespace network {

// This corresponds to network::mojom::CrossOriginEmbedderPolicy.
// See the comments there.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) CrossOriginEmbedderPolicy final {
  CrossOriginEmbedderPolicy();
  ~CrossOriginEmbedderPolicy();
  CrossOriginEmbedderPolicy(const CrossOriginEmbedderPolicy&);
  CrossOriginEmbedderPolicy(CrossOriginEmbedderPolicy&&);
  CrossOriginEmbedderPolicy& operator=(const CrossOriginEmbedderPolicy&);
  CrossOriginEmbedderPolicy& operator=(CrossOriginEmbedderPolicy&&);
  bool operator==(const CrossOriginEmbedderPolicy&) const;

  mojom::CrossOriginEmbedderPolicyValue value =
      mojom::CrossOriginEmbedderPolicyValue::kNone;
  base::Optional<std::string> reporting_endpoint;
  mojom::CrossOriginEmbedderPolicyValue report_only_value =
      mojom::CrossOriginEmbedderPolicyValue::kNone;
  base::Optional<std::string> report_only_reporting_endpoint;

  // Parses |header_value| and returns a pair of a COEP value and an optional
  // reporting endpoint. This is usually used for two headers.
  //
  //   CrossOriginEmbedderPolicyValue coep;
  //   std::tie(coep.value, coep.reporting_endpoint) =
  //     CrossOriginEmbedderPolicyValue::Parse(header_value);
  //   std::tie(coep.report_only_value, coep.report_only_reporting_endpoint) =
  //     CrossOriginEmbedderPolicyValue::Parse(report_only_header_value);
  static std::pair<mojom::CrossOriginEmbedderPolicyValue,
                   base::Optional<std::string>>
  Parse(base::StringPiece header_value);

  static const char kHeaderName[];
  static const char kReportOnlyHeaderName[];
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_
