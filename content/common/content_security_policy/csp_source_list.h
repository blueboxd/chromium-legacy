// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
#define CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_

#include <vector>

#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// TODO(arthursonzogni): Once CSPContext has been moved to
// /services/network/public/content_security_policy, this file is going to be
// moved there as well.
class CSPContext;

std::string CONTENT_EXPORT
ToString(const network::mojom::CSPSourceListPtr& source_list);

// Return true when at least one source in the |source_list| matches the
// |url| for a given |context|.
bool CONTENT_EXPORT
CheckCSPSourceList(const network::mojom::CSPSourceListPtr& source_list,
                   const GURL& url,
                   CSPContext* context,
                   bool has_followed_redirect = false,
                   bool is_response_check = false);

}  // namespace content
#endif  // CONTENT_COMMON_CONTENT_SECURITY_POLICY_CSP_SOURCE_LIST_H_
