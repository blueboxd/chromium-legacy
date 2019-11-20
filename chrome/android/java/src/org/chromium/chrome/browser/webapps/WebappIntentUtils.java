// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Utility methods for extracting data from Homescreen-shortcut/WebAPK launch intent.
 */
public class WebappIntentUtils {
    /**
     * Converts color from signed Integer where an unspecified color is represented as null to
     * to unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING}.
     */
    public static long colorFromIntegerColor(Integer color) {
        if (color != null) {
            return color.intValue();
        }
        return ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
    }

    public static boolean isLongColorValid(long longColor) {
        return (longColor != ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
    }

    /**
     * Converts color from unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING} to a signed Integer where an
     * unspecified color is represented as null.
     */
    public static Integer colorFromLongColor(long longColor) {
        return isLongColorValid(longColor) ? Integer.valueOf((int) longColor) : null;
    }

    public static String idFromIntent(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
    }

    /**
     * Returns the WebAPK's shell launch timestamp associated with the passed-in intent, or -1.
     */
    public static long getWebApkShellLaunchTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
    }

    /**
     * Copies the WebAPKs's shell launch timestamp, if set, from {@link fromIntent} to
     * {@link toIntent}.
     */
    public static void copyWebApkShellLaunchTime(Intent fromIntent, Intent toIntent) {
        toIntent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, getWebApkShellLaunchTime(fromIntent));
    }

    /**
     * Returns the timestamp when the WebAPK shell showed the splash screen. Returns -1 if the
     * WebAPK shell did not show the splash screen.
     */
    public static long getNewStyleWebApkSplashShownTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME, -1);
    }

    /**
     * Copies the timestamp, if set, that the WebAPK shell showed the splash screen from
     * {@link fromIntent} to {@link toIntent}.
     */
    public static void copyNewStyleWebApkSplashShownTime(Intent fromIntent, Intent toIntent) {
        toIntent.putExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME,
                getNewStyleWebApkSplashShownTime(fromIntent));
    }
}
