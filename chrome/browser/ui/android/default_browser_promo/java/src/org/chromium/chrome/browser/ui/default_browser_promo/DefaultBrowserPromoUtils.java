// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.app.Activity;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A utility class providing information regarding states of default browser.
 */
public class DefaultBrowserPromoUtils {
    @IntDef({DefaultBrowserState.CHROME_DEFAULT, DefaultBrowserState.NO_DEFAULT,
            DefaultBrowserState.OTHER_DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserState {
        int NO_DEFAULT = 0;
        int OTHER_DEFAULT = 1;
        /**
         * CHROME_DEFAULT means the currently running Chrome as opposed to
         * #isCurrentDefaultBrowserChrome() which looks for any Chrome.
         */
        int CHROME_DEFAULT = 2;
    }

    private static final int MIN_TRIGGER_SESSION_COUNT = 3;
    private static final String SESSION_COUNT_PARAM = "min_trigger_session_count";

    private static final String CHROME_STABLE_PACKAGE_NAME = "com.android.chrome";

    // TODO(crbug.com/1090103): move to some util class for reuse.
    private static final String[] CHROME_PACKAGE_NAMES = {CHROME_STABLE_PACKAGE_NAME,
            "org.chromium.chrome", "com.chrome.canary", "com.chrome.beta", "com.chrome.dev"};

    /**
     * Determine whether a promo dialog should be displayed or not. And prepare related logic to
     * launch promo if a promo dialog has been decided to display.
     * Return false if any of following criteria is met:
     *      1. A promo dialog has been displayed before.
     *      2. Not enough sessions have been started before.
     *      3. Any chrome, including pre-stable, has been set as default.
     *      4. On Chrome stable while no default browser is set and multiple chrome channels
     *         are installed.
     *
     * @param activity The context.
     * @return True if promo dialog will be displayed.
     */
    public static boolean prepareLaunchPromoIfNeeded(Activity activity) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)) {
            return false;
        }

        // Criteria 1
        // TODO(crbug.com/1090103): change to int if dialog will be re-promo.
        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_IS_PROMOED, false)) {
            return false;
        }

        // Criteria 2
        int minSessionCount = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO, SESSION_COUNT_PARAM,
                MIN_TRIGGER_SESSION_COUNT);

        if (SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT, 0)
                < minSessionCount) {
            return false;
        }

        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        int state = getCurrentDefaultBrowserState(info);

        // Already default
        if (state == DefaultBrowserState.CHROME_DEFAULT) return false;

        // Criteria 3
        if (state == DefaultBrowserState.OTHER_DEFAULT && isCurrentDefaultBrowserChrome(info)) {
            return false;
        }

        // Criteria 4
        if (ContextUtils.getApplicationContext().getPackageName().equals(CHROME_STABLE_PACKAGE_NAME)
                && isChromePreStableInstalled()
                && state == DefaultBrowserState.NO_DEFAULT) {
            return false;
        }

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_IS_PROMOED, true);
        DefaultBrowserPromoManager.create(activity).promo(state);
        return true;
    }

    /**
     * Increment session count for triggering feature in the future.
     */
    public static void incrementSessionCount() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_DEFAULT_BROWSER_PROMO)) return;
        SharedPreferencesManager.getInstance().incrementInt(
                ChromePreferenceKeys.DEFAULT_BROWSER_PROMO_SESSION_COUNT);
    }

    private static boolean isChromePreStableInstalled() {
        for (ResolveInfo info : PackageManagerUtils.queryAllWebBrowsersInfo()) {
            for (String name : CHROME_PACKAGE_NAMES) {
                if (name.equals(CHROME_STABLE_PACKAGE_NAME)) continue;
                if (name.equals(info.activityInfo.packageName)) return true;
            }
        }
        return false;
    }

    private static boolean isCurrentDefaultBrowserChrome(ResolveInfo info) {
        String packageName = info.activityInfo.packageName;
        for (String name : CHROME_PACKAGE_NAMES) {
            if (name.equals(packageName)) return true;
        }
        return false;
    }

    @DefaultBrowserState
    private static int getCurrentDefaultBrowserState(ResolveInfo info) {
        if (info.match == 0) return DefaultBrowserState.NO_DEFAULT; // no default
        if (TextUtils.equals(ContextUtils.getApplicationContext().getPackageName(),
                    info.activityInfo.packageName)) {
            return DefaultBrowserState.CHROME_DEFAULT; // Already default
        }
        return DefaultBrowserState.OTHER_DEFAULT;
    }

    /**
     * The current {@link DefaultBrowserState} in the system.
     */
    @DefaultBrowserState
    public static int getCurrentDefaultBrowserState() {
        ResolveInfo info = PackageManagerUtils.resolveDefaultWebBrowserActivity();
        return getCurrentDefaultBrowserState(info);
    }
}
