// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
     * To be called when an origin is verified with a package.
     */
    public void onOriginVerified(Origin origin, String packageName) {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.TRUSTED_WEB_ACTIVITY_LOCATION_DELEGATION)) {
            return;
        }
        // TODO(crbug.com/1069506): add implementations here.
    }
}
