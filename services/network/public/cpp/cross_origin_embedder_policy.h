// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_

#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-shared.h"

namespace network {

// This corresponds to network::mojom::CrossOriginEmbedderPolicyWithReporting.
// See the comments there.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    CrossOriginEmbedderPolicyWithReporting final {
  CrossOriginEmbedderPolicyWithReporting();
  ~CrossOriginEmbedderPolicyWithReporting();
  CrossOriginEmbedderPolicyWithReporting(
      const CrossOriginEmbedderPolicyWithReporting&);
  CrossOriginEmbedderPolicyWithReporting(
      CrossOriginEmbedderPolicyWithReporting&&);
  CrossOriginEmbedderPolicyWithReporting& operator=(
      const CrossOriginEmbedderPolicyWithReporting&);
  CrossOriginEmbedderPolicyWithReporting& operator=(
      CrossOriginEmbedderPolicyWithReporting&&);

  mojom::CrossOriginEmbedderPolicy value =
      mojom::CrossOriginEmbedderPolicy::kNone;
  base::Optional<std::string> reporting_endpoint;
  mojom::CrossOriginEmbedderPolicy report_only_value =
      mojom::CrossOriginEmbedderPolicy::kNone;
  base::Optional<std::string> report_only_reporting_endpoint;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_H_
