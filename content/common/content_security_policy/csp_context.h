// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_

#include <vector>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/navigation_params.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A CSPContext represents the system on which the Content-Security-Policy are
// enforced. One must define via its virtual methods how to report violations
// and what is the set of scheme that bypass the CSP. Its main implementation
// is in content/browser/frame_host/render_frame_host_impl.h
class CONTENT_EXPORT CSPContext {
 public:
  // This enum represents what set of policies should be checked by
  // IsAllowedByCsp().
  enum CheckCSPDisposition {
    // Only check report-only policies.
    CHECK_REPORT_ONLY_CSP,
    // Only check enforced policies. (Note that enforced policies can still
    // trigger reports.)
    CHECK_ENFORCED_CSP,
    // Check all policies.
    CHECK_ALL_CSP,
  };

  CSPContext();
  virtual ~CSPContext();

  // Check if an |url| is allowed by the set of Content-Security-Policy. It will
  // report any violation by:
  // * displaying a console message.
  // * triggering the "SecurityPolicyViolation" javascript event.
  // * sending a JSON report to any uri defined with the "report-uri" directive.
  // Returns true when the request can proceed, false otherwise.
  bool IsAllowedByCsp(network::mojom::CSPDirectiveName directive_name,
                      const GURL& url,
                      bool has_followed_redirect,
                      bool is_response_check,
                      const network::mojom::SourceLocationPtr& source_location,
                      CheckCSPDisposition check_csp_disposition,
                      bool is_form_submission);

  void SetSelf(const url::Origin origin);
  void SetSelf(network::mojom::CSPSourcePtr self_source);

  // When a CSPSourceList contains 'self', the url is allowed when it match the
  // CSPSource returned by this function.
  // Sometimes there is no 'self' source. It means that the current origin is
  // unique and no urls will match 'self' whatever they are.
  // Note: When there is a 'self' source, its scheme is guaranteed to be
  // non-empty.
  const network::mojom::CSPSourcePtr& self_source() { return self_source_; }

  virtual void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation);

  void ResetContentSecurityPolicies() { policies_.clear(); }
  void AddContentSecurityPolicy(
      network::mojom::ContentSecurityPolicyPtr policy) {
    policies_.push_back(std::move(policy));
  }
  const std::vector<network::mojom::ContentSecurityPolicyPtr>&
  ContentSecurityPolicies() {
    return policies_;
  }

  virtual bool SchemeShouldBypassCSP(const base::StringPiece& scheme);

  // For security reasons, some urls must not be disclosed cross-origin in
  // violation reports. This includes the blocked url and the url of the
  // initiator of the navigation. This information is potentially transmitted
  // between different renderer processes.
  // TODO(arthursonzogni): Stop hiding sensitive parts of URLs in console error
  // messages as soon as there is a way to send them to the devtools process
  // without the round trip in the renderer process.
  // See https://crbug.com/721329
  virtual void SanitizeDataForUseInCspViolation(
      bool has_followed_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const;

 private:
  // TODO(arthursonzogni): This is an interface, stop storing data.
  network::mojom::CSPSourcePtr self_source_;  // Nullable.
  std::vector<network::mojom::ContentSecurityPolicyPtr> policies_;

  DISALLOW_COPY_AND_ASSIGN(CSPContext);
};

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_CONTEXT_H_
