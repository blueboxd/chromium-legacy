// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

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
    network::mojom::ContentSecurityPolicyType type,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out);

COMPONENT_EXPORT(NETWORK_CPP)
std::string ToString(mojom::CSPDirectiveName name);

COMPONENT_EXPORT(NETWORK_CPP)
mojom::CSPDirectiveName ToCSPDirectiveName(const std::string& name);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_
