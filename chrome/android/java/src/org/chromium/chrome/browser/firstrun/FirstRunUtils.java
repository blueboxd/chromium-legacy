// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.signin.AccountManagerFacade;

/** Provides first run related utility functions. */
public class FirstRunUtils {
    public static final String CACHED_TOS_ACCEPTED_PREF = "first_run_tos_accepted";
    private static Boolean sHasGoogleAccountAuthenticator;

    /**
     * Synchronizes first run native and Java preferences.
     * Must be called after native initialization.
     */
    public static void cacheFirstRunPrefs() {
        SharedPreferences javaPrefs = ContextUtils.getAppSharedPreferences();
        PrefServiceBridge prefsBridge = PrefServiceBridge.getInstance();
        // Set both Java and native prefs if any of the three indicators indicate ToS has been
        // accepted. This needed because:
        //   - Old versions only set native pref, so this syncs Java pref.
        //   - Backup & restore does not restore native pref, so this needs to update it.
        //   - checkAnyUserHasSeenToS() may be true which needs to sync its state to the prefs.
        boolean javaPrefValue = javaPrefs.getBoolean(CACHED_TOS_ACCEPTED_PREF, false);
        boolean nativePrefValue = prefsBridge.isFirstRunEulaAccepted();
        boolean userHasSeenTos =
                ToSAckedReceiver.checkAnyUserHasSeenToS();
        boolean isFirstRunComplete = FirstRunStatus.getFirstRunFlowComplete();
        if (javaPrefValue || nativePrefValue || userHasSeenTos || isFirstRunComplete) {
            if (!javaPrefValue) {
                javaPrefs.edit().putBoolean(CACHED_TOS_ACCEPTED_PREF, true).apply();
            }
            if (!nativePrefValue) {
                prefsBridge.setEulaAccepted();
            }
        }
    }

    /**
     * @return Whether the user has accepted Chrome Terms of Service.
     */
    public static boolean didAcceptTermsOfService() {
        // Note: Does not check PrefServiceBridge.getInstance().isFirstRunEulaAccepted()
        // because this may be called before native is initialized.
        return ContextUtils.getAppSharedPreferences().getBoolean(CACHED_TOS_ACCEPTED_PREF, false)
                || ToSAckedReceiver.checkAnyUserHasSeenToS();
    }

    /**
     * Sets the EULA/Terms of Services state as "ACCEPTED".
     * @param allowCrashUpload True if the user allows to upload crash dumps and collect stats.
     */
    public static void acceptTermsOfService(boolean allowCrashUpload) {
        UmaSessionStats.changeMetricsReportingConsent(allowCrashUpload);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(CACHED_TOS_ACCEPTED_PREF, true)
                .apply();
        PrefServiceBridge.getInstance().setEulaAccepted();
    }

    /**
     * Determines whether or not the user has a Google account (so we can sync) or can add one.
     * @return Whether or not sync is allowed on this device.
     */
    static boolean canAllowSync() {
        return (hasGoogleAccountAuthenticator() && hasSyncPermissions()) || hasGoogleAccounts();
    }

    @VisibleForTesting
    static boolean hasGoogleAccountAuthenticator() {
        if (sHasGoogleAccountAuthenticator == null) {
            AccountManagerFacade accountHelper = AccountManagerFacade.get();
            sHasGoogleAccountAuthenticator = accountHelper.hasGoogleAccountAuthenticator();
        }
        return sHasGoogleAccountAuthenticator;
    }

    @VisibleForTesting
    static boolean hasGoogleAccounts() {
        return AccountManagerFacade.get().hasGoogleAccounts();
    }

    @SuppressLint("InlinedApi")
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static boolean hasSyncPermissions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return true;

        UserManager manager = (UserManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USER_SERVICE);
        Bundle userRestrictions = manager.getUserRestrictions();
        return !userRestrictions.getBoolean(UserManager.DISALLOW_MODIFY_ACCOUNTS, false);
    }
}
