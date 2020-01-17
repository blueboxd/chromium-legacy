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

// This class is a thin wrapper around mojom::ContentSecurityPolicy.
class COMPONENT_EXPORT(NETWORK_CPP) ContentSecurityPolicy {
 public:
  ContentSecurityPolicy();
  ~ContentSecurityPolicy();

  ContentSecurityPolicy(const ContentSecurityPolicy&) = delete;
  ContentSecurityPolicy& operator=(const ContentSecurityPolicy&) = delete;

  // Parses the Content-Security-Policy headers specified in |headers| while
  // requesting |request_url|. The |request_url| is used for violation
  // reporting, as specified in
  // https://w3c.github.io/webappsec-csp/#report-violation.
  void Parse(const GURL& request_url, const net::HttpResponseHeaders& headers);

  // Parses a Content-Security-Policy |header|.
  void Parse(const GURL& base_url,
             network::mojom::ContentSecurityPolicyType type,
             base::StringPiece header);

  const std::vector<mojom::ContentSecurityPolicyPtr>&
  content_security_policies() {
    return content_security_policies_;
  }
  std::vector<mojom::ContentSecurityPolicyPtr> TakeContentSecurityPolicy() {
    return std::move(content_security_policies_);
  }

  // string <-> enum conversions:
  static std::string ToString(mojom::CSPDirectiveName);
  static mojom::CSPDirectiveName ToDirectiveName(const std::string&);

 private:
  std::vector<mojom::ContentSecurityPolicyPtr> content_security_policies_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_SECURITY_POLICY_H_
