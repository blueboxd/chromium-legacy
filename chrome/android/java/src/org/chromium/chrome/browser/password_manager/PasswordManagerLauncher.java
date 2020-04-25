// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import com.google.android.gms.common.ConnectionResult;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Bridge between Java and native PasswordManager code.
 */
public class PasswordManagerLauncher {
    private static final String GOOGLE_ACCOUNT_PWM_UI = "google-password-manager";

    // Name of the parameter for the google-password-manager feature, used to override the default
    // minimum version for Google Play Services.
    private static final String MIN_GOOGLE_PLAY_SERVICES_VERSION_PARAM =
            "min-google-play-services-version";

    // Default value for the minimum version for Google Play Services, such that the Google Account
    // password manager is available. Set to v21.
    // This can be overridden via Finch.
    private static final int DEFAULT_MIN_GOOGLE_PLAY_SERVICES_APK_VERSION = 13400000;

    private PasswordManagerLauncher() {}

    /**
     * Launches the password settings in or the Google Password Manager if available.
     * @param activity used to show the UI to manage passwords.
     */
    public static void showPasswordSettings(
            Activity activity, @ManagePasswordsReferrer int referrer) {
        RecordHistogram.recordEnumeratedHistogram("PasswordManager.ManagePasswordsReferrer",
                referrer, ManagePasswordsReferrer.MAX_VALUE + 1);
        if (isSyncingPasswordsWithoutCustomPassphrase()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "PasswordManager.ManagePasswordsReferrerSignedInAndSyncing", referrer,
                    ManagePasswordsReferrer.MAX_VALUE + 1);
            if (!PrefServiceBridge.getInstance().isManagedPreference(
                        Pref.REMEMBER_PASSWORDS_ENABLED)) {
                if (tryShowingTheGooglePasswordManager(activity)) return;
            }
        }

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(activity, PasswordSettings.class);
    }

    @CalledByNative
    private static void showPasswordSettings(
            WebContents webContents, @ManagePasswordsReferrer int referrer) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        WeakReference<Activity> currentActivity = window.getActivity();
        showPasswordSettings(currentActivity.get(), referrer);
    }

    public static boolean isSyncingPasswordsWithoutCustomPassphrase() {
        if (!IdentityServicesProvider.get().getIdentityManager().hasPrimaryAccount()) return false;

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null
                || !profileSyncService.getActiveDataTypes().contains(ModelType.PASSWORDS)) {
            return false;
        }

        if (profileSyncService.isUsingSecondaryPassphrase()) return false;

        return true;
    }

    private static boolean tryShowingTheGooglePasswordManager(Activity activity) {
        GooglePasswordManagerUIProvider googlePasswordManagerUIProvider =
                AppHooks.get().createGooglePasswordManagerUIProvider();
        if (googlePasswordManagerUIProvider == null) return false;

        int minGooglePlayServicesVersion = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                GOOGLE_ACCOUNT_PWM_UI, MIN_GOOGLE_PLAY_SERVICES_VERSION_PARAM,
                DEFAULT_MIN_GOOGLE_PLAY_SERVICES_APK_VERSION);
        if (AppHooks.get().isGoogleApiAvailableWithMinApkVersion(minGooglePlayServicesVersion)
                != ConnectionResult.SUCCESS) {
            return false;
        }

        if (!ChromeFeatureList.isEnabled(GOOGLE_ACCOUNT_PWM_UI)) return false;

        return googlePasswordManagerUIProvider.showGooglePasswordManager(activity);
    }
}
