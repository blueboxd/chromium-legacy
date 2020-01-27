// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.geolocation;

import android.Manifest;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.location.LocationSettingsDialogContext;
import org.chromium.components.location.LocationSettingsDialogOutcome;
import org.chromium.components.location.LocationUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides native access to system-level location settings and permissions.
 */
public class LocationSettings {
    private LocationSettings() {}

    @CalledByNative
    private static boolean hasAndroidLocationPermission() {
        return LocationUtils.getInstance().hasAndroidLocationPermission();
    }

    @CalledByNative
    private static boolean canPromptForAndroidLocationPermission(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;

        return windowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @CalledByNative
    private static boolean isSystemLocationSettingEnabled() {
        return LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @CalledByNative
    private static boolean canPromptToEnableSystemLocationSetting() {
        return LocationUtils.getInstance().canPromptToEnableSystemLocationSetting();
    }

    @CalledByNative
    private static void promptToEnableSystemLocationSetting(
            @LocationSettingsDialogContext int promptContext, WebContents webContents,
            final long nativeCallback) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            LocationSettingsJni.get().onLocationSettingsDialogOutcome(
                    nativeCallback, LocationSettingsDialogOutcome.NO_PROMPT);
            return;
        }
        LocationUtils.getInstance().promptToEnableSystemLocationSetting(
                promptContext, window, new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        LocationSettingsJni.get().onLocationSettingsDialogOutcome(
                                nativeCallback, result);
                    }
                });
    }

    @NativeMethods
    interface Natives {
        void onLocationSettingsDialogOutcome(long callback, int result);
    }
}
