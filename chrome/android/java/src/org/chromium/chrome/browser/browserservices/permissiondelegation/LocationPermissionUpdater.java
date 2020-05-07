// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * This class updates the location permission for an Origin based on the location permission
 * that the linked TWA has in Android.
 *
 * TODO(eirage): Add a README.md for Location Delegation.
 */
@Singleton
public class LocationPermissionUpdater {
    private static final String TAG = "TWALocations";

    private static final @ContentSettingsType int TYPE = ContentSettingsType.GEOLOCATION;

    private final TrustedWebActivityPermissionManager mPermissionManager;
    private final TrustedWebActivityClient mTrustedWebActivityClient;

    @Inject
    public LocationPermissionUpdater(TrustedWebActivityPermissionManager permissionManager,
            TrustedWebActivityClient trustedWebActivityClient) {
        mPermissionManager = permissionManager;
        mTrustedWebActivityClient = trustedWebActivityClient;
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService
     * for the origin, or the client app does not support location delegation, return the
     * origin's location permission to what it was before any client app was installed.
     */
    void onClientAppUninstalled(Origin origin) {
        mPermissionManager.resetStoredPermission(origin, TYPE);
    }
}
