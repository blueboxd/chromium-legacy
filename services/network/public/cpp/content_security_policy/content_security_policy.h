// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}

namespace network {
class CSPContext;

// Parses the Content-Security-Policy headers specified in |headers| and appends
// the results into |out|.
//
// The |base_url| is used for resolving the URLs in the 'report-uri' directives.
// See https://w3c.github.io/webappsec-csp/#report-violation.
COMPONENT_EXPORT(NETWORK_CPP)
void AddContentSecurityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out);

COMPONENT_EXPORT(NETWORK_CPP)
void AddContentSecurityPolicyFromHeaders(
    base::StringPiece header,
    mojom::ContentSecurityPolicyType type,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out);

// Return true when the |policy| allows a request to the |url| in relation to
// the |directive| for a given |context|.
// Note: Any policy violation are reported to the |context|.
COMPONENT_EXPORT(NETWORK_CPP)
bool CheckContentSecurityPolicy(const mojom::ContentSecurityPolicyPtr& policy,
                                mojom::CSPDirectiveName directive,
                                const GURL& url,
                                bool has_followed_redirect,
                                bool is_response_check,
                                CSPContext* context,
                                const mojom::SourceLocationPtr& source_location,
                                bool is_form_submission);

// Return true if the set of |policies| contains one "Upgrade-Insecure-request"
// directive.
COMPONENT_EXPORT(NETWORK_CPP)
bool ShouldUpgradeInsecureRequest(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies);

// Upgrade scheme of the |url| from HTTP to HTTPS and its port from 80 to 433
// (if needed). This is a no-op on non-HTTP and on potentially trustworthy URL.
COMPONENT_EXPORT(NETWORK_CPP)
void UpgradeInsecureRequest(GURL* url);

COMPONENT_EXPORT(NETWORK_CPP)
mojom::CSPDirectiveName ToCSPDirectiveName(const std::string& name);

COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(mojom::CSPDirectiveName name);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_CONTENT_SECURITY_POLICY_H_
