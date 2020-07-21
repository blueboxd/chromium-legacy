// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;

/**
 * PrefServiceBridge is a singleton which provides read and write access to native PrefService.
 */
public class PrefServiceBridge {
    // Singleton constructor. Do not call directly unless for testing purpose.
    @VisibleForTesting
    protected PrefServiceBridge() {}

    private static PrefServiceBridge sInstance;

    /**
     * @return The singleton PrefServiceBridge instance.
     */
    public static PrefServiceBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new PrefServiceBridge();
        }
        return sInstance;
    }

    /**
     * @param preference The name of the preference.
     */
    public void clearPref(@NonNull String preference) {
        PrefServiceBridgeJni.get().clearPref(preference);
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is enabled.
     */
    public boolean getBoolean(@NonNull String preference) {
        return PrefServiceBridgeJni.get().getBoolean(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setBoolean(@NonNull String preference, boolean value) {
        PrefServiceBridgeJni.get().setBoolean(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    public int getInteger(@NonNull String preference) {
        return PrefServiceBridgeJni.get().getInteger(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setInteger(@NonNull String preference, int value) {
        PrefServiceBridgeJni.get().setInteger(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return value The value of the specified preference.
     */
    @NonNull
    public String getString(@NonNull String preference) {
        return PrefServiceBridgeJni.get().getString(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setString(@NonNull String preference, @NonNull String value) {
        PrefServiceBridgeJni.get().setString(preference, value);
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is managed.
     */
    public boolean isManagedPreference(@NonNull String preference) {
        return PrefServiceBridgeJni.get().isManagedPreference(preference);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(@Nullable PrefServiceBridge instanceForTesting) {
        sInstance = instanceForTesting;
    }

    @NativeMethods
    interface Natives {
        void clearPref(String preference);
        boolean getBoolean(String preference);
        void setBoolean(String preference, boolean value);
        int getInteger(String preference);
        void setInteger(String preference, int value);
        String getString(String preference);
        void setString(String preference, String value);
        boolean isManagedPreference(String preference);
    }
}
