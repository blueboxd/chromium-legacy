// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_

#include <string>

#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// TODO(arthursonzogni): Once CSPContext has been moved to
// /services/network/public/content_security_policy, this file is going to be
// moved there as well.
class CSPContext;

// Check if a |url| matches with a CSP |source| matches.
bool CONTENT_EXPORT CheckCSPSource(const network::mojom::CSPSourcePtr& source,
                                   const GURL& url,
                                   CSPContext* context,
                                   bool has_followed_redirect = false);

// Serialize the CSPSource |source| as a string. This is used for reporting
// violations.
std::string CONTENT_EXPORT ToString(const network::mojom::CSPSourcePtr& source);

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_H_
