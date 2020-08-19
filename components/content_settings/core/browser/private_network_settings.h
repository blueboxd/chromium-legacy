// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_

#include "components/content_settings/core/common/content_settings.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

class GURL;
class HostContentSettingsMap;

namespace content_settings {

// Returns the private network request policy to use for |url| given |map|.
//
// |map| must not be nullptr. Caller retains ownership.
// |url| should identify the frame initiating a request.
network::mojom::PrivateNetworkRequestPolicy GetPrivateNetworkRequestPolicy(
    const HostContentSettingsMap* map,
    const GURL& url);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
