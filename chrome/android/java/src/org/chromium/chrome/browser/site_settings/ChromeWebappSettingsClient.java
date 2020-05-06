// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.browser_ui.site_settings.WebappSettingsClient;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/**
 * A SiteSettingsClient instance that contains Chrome-specific Site Settings logic.
 */
public class ChromeWebappSettingsClient implements WebappSettingsClient {
    @Override
    public Set<String> getOriginsWithInstalledApp() {
        WebappRegistry registry = WebappRegistry.getInstance();
        return registry.getOriginsWithInstalledApp();
    }

    @Override
    public Set<String> getAllDelegatedNotificationOrigins() {
        return TrustedWebActivityPermissionManager.get().getAllDelegatedOrigins();
    }

    @Override
    @Nullable
    public String getNotificationDelegateAppNameForOrigin(Origin origin) {
        return TrustedWebActivityPermissionManager.get().getDelegateAppName(origin);
    }

    @Override
    @Nullable
    public String getNotificationDelegatePackageNameForOrigin(Origin origin) {
        return TrustedWebActivityPermissionManager.get().getDelegatePackageName(origin);
    }
}
