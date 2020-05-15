// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.ComponentName;

import androidx.annotation.WorkerThread;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.UiThreadTaskTraits;

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

    /**
     * To be called when a client app is requesting location. It check and sets the location
     * permission for that origin to client app's Android location permission if a
     * TrustedWebActivityService is found, and the TWAService supports location permission.
     */
    void checkPermission(Origin origin, long callback) {
        boolean couldConnect = mTrustedWebActivityClient.checkLocationPermission(
                origin, (app, enabled) -> updatePermission(origin, callback, app, enabled));
        if (!couldConnect) {
            mPermissionManager.resetStoredPermission(origin, TYPE);
            InstalledWebappBridge.onGetPermissionResult(callback, false);
        }
    }

    @WorkerThread
    private void updatePermission(
            Origin origin, long callback, ComponentName app, boolean enabled) {
        // This method will be called by the TrustedWebActivityClient on a background thread, so
        // hop back over to the UI thread to deal with the result.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
            mPermissionManager.updatePermission(origin, app.getPackageName(), TYPE, enabled);
            Log.d(TAG, "Updating origin location permissions to: %b", enabled);

            InstalledWebappBridge.onGetPermissionResult(callback, enabled);
        });
    }
}
