// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CONTENT_SECURITY_POLICY_H_
#define SERVICES_NETWORK_CONTENT_SECURITY_POLICY_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) ContentSecurityPolicy {
 public:
  ContentSecurityPolicy();
  ~ContentSecurityPolicy();

  // Parses the Content-Security-Policy headers specified in |headers|.
  bool Parse(const net::HttpResponseHeaders& headers);

  const mojom::CSPSourceListPtr& frame_ancestors() { return frame_ancestors_; }

 private:
  friend int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
  bool Parse(base::StringPiece header);

  // Parses the frame-ancestor directive of a Content-Security-Policy header.
  bool ParseFrameAncestors(base::StringPiece header_value);

  mojom::CSPSourceListPtr frame_ancestors_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_CONTENT_SECURITY_POLICY_H_
