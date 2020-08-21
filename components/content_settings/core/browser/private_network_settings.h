// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_

class GURL;
class HostContentSettingsMap;

namespace content_settings {

// Returns whether |url| should be allowed to make insecure private network
// requests, given the settings contained in |map|.
//
// |map| must not be nullptr. Caller retains ownership.
// |url| should identify the frame initiating a request.
bool ShouldAllowInsecurePrivateNetworkRequests(
    const HostContentSettingsMap* map,
    const GURL& url);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PRIVATE_NETWORK_SETTINGS_H_
