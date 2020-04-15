// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/**
 * A SiteSettingsClient instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeNotificationSettingsClient implements NotificationSettingsClient {
    @Override
    public Set<String> getAllDelegatedOrigins() {
        return TrustedWebActivityPermissionManager.get().getAllDelegatedOrigins();
    }

    @Override
    @Nullable
    public String getDelegateAppNameForOrigin(Origin origin) {
        return TrustedWebActivityPermissionManager.get().getDelegateAppName(origin);
    }

    @Override
    @Nullable
    public String getDelegatePackageNameForOrigin(Origin origin) {
        return TrustedWebActivityPermissionManager.get().getDelegatePackageName(origin);
    }

    @Override
    public String getChannelIdForOrigin(String origin) {
        return SiteChannelsManager.getInstance().getChannelIdForOrigin(origin);
    }
}
