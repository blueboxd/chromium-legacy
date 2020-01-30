// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace content {

// TODO(arthursonzogni): Once CSPContext has been moved to
// /services/network/public/content_security_policy, this file is going to be
// moved there as well.
class CSPContext;
struct SourceLocation;

// Return true when the |policy| allows a request to the |url| in relation to
// the |directive| for a given |context|.
// Note: Any policy violation are reported to the |context|.
bool CONTENT_EXPORT CheckContentSecurityPolicy(
    const network::mojom::ContentSecurityPolicyPtr& policy,
    network::mojom::CSPDirectiveName directive,
    const GURL& url,
    bool has_followed_redirect,
    bool is_response_check,
    CSPContext* context,
    const SourceLocation& source_location,
    bool is_form_submission);

// Returns true if |policy| specifies that an insecure HTTP request should be
// upgraded to HTTPS.
bool CONTENT_EXPORT ShouldUpgradeInsecureRequest(
    const network::mojom::ContentSecurityPolicyPtr& policy);

std::string CONTENT_EXPORT ToString(const network::mojom::CSPDirectivePtr&);

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
