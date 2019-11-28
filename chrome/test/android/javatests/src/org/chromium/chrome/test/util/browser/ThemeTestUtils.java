// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.os.Build;

import org.junit.Assert;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Utility methods for tests which customize the tab's theme color.
 */
public class ThemeTestUtils {
    /**
     * Waits for the activity active tab's theme-color to change to the passed-in color.
     */
    public static void waitForThemeColor(ChromeActivity activity, int expectedColor)
            throws ExecutionException, TimeoutException {
        waitForThemeColor(activity, expectedColor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    public static void waitForThemeColor(ChromeActivity activity, int expectedColor, long timeoutMs)
            throws ExecutionException, TimeoutException {
        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedColor,
                        () -> TabThemeColorHelper.getColor(activity.getActivityTab())),
                timeoutMs, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Asserts that the status bar color equals the passed-in color.
     */
    public static void assertStatusBarColor(ChromeActivity activity, int expectedColor) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            expectedColor = ColorUtils.getDarkenedColorForStatusBar(expectedColor);
        }
        Assert.assertEquals(expectedColor, activity.getWindow().getStatusBarColor());
    }
}
