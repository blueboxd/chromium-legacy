// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "content/common/navigation_params.mojom.h"

namespace content {

SourceLocation::SourceLocation() = default;

SourceLocation::SourceLocation(const std::string& url,
                               unsigned int line_number,
                               unsigned int column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

SourceLocation::~SourceLocation() = default;

mojom::InitiatorCSPInfoPtr CreateInitiatorCSPInfo() {
  return mojom::InitiatorCSPInfo::New(
      network::mojom::CSPDisposition::CHECK,
      std::vector<network::mojom::ContentSecurityPolicyPtr>() /* empty */,
      nullptr /* initiator_self_source */
  );
}

mojom::CommonNavigationParamsPtr CreateCommonNavigationParams() {
  auto common_params = mojom::CommonNavigationParams::New();
  common_params->referrer = blink::mojom::Referrer::New();
  common_params->navigation_start = base::TimeTicks::Now();
  common_params->initiator_csp_info = CreateInitiatorCSPInfo();

  return common_params;
}

mojom::CommitNavigationParamsPtr CreateCommitNavigationParams() {
  auto commit_params = mojom::CommitNavigationParams::New();
  commit_params->navigation_token = base::UnguessableToken::Create();
  commit_params->navigation_timing = mojom::NavigationTiming::New();

  return commit_params;
}

}  // namespace content
