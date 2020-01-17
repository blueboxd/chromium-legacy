// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_directive.h"
#include "services/network/public/cpp/content_security_policy.h"

namespace content {

CSPDirective::CSPDirective() = default;

CSPDirective::CSPDirective(network::mojom::CSPDirectiveName name,
                           const CSPSourceList& source_list)
    : name(name), source_list(source_list) {}

CSPDirective::CSPDirective(const CSPDirective&) = default;

CSPDirective::CSPDirective(network::mojom::CSPDirectivePtr directive)
    : name(directive->name), source_list(std::move(directive->source_list)) {}

std::string CSPDirective::ToString() const {
  return network::ContentSecurityPolicy::ToString(name) + " " +
         source_list.ToString();
}

}  // namespace content
