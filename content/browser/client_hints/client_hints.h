// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "content/public/browser/client_hints_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"

class BrowserContext;
class FrameTreeNode;

namespace content {

// Returns |rtt| after adding host-specific random noise, and rounding it as
// per the NetInfo spec to improve privacy.
CONTENT_EXPORT unsigned long RoundRttForTesting(
    const std::string& host,
    const base::Optional<base::TimeDelta>& rtt);

// Returns downlink (in Mbps) after adding host-specific random noise to
// |downlink_kbps| (which is in Kbps), and rounding it as per the NetInfo spec
// to improve privacy.
CONTENT_EXPORT double RoundKbpsToMbpsForTesting(
    const std::string& host,
    const base::Optional<int32_t>& downlink_kbps);

// Updates the user agent client hint headers. This is called if the value of
// |override_ua| changes after the NavigationRequest was created.
CONTENT_EXPORT void UpdateNavigationRequestClientUaHeaders(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers);

CONTENT_EXPORT void AddNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode*);

CONTENT_EXPORT void PersistAcceptCHAfterNagivationRequestRedirect(
    const GURL& url,
    const ::network::mojom::ParsedHeadersPtr& headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode*);

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
