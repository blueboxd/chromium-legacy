// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;

/**
 * Contains logic that decides whether to enable features related to tabs.
 */
public class TabFeatureUtilities {
    private static final String TAG = "TabFeatureUtilities";
    private static final int DEFAULT_MIN_SDK = Build.VERSION_CODES.O;
    private static final int DEFAULT_MIN_MEMORY_MB = 2048;

    // Field trial parameter for the minimum Android SDK version to enable zooming animation.
    public static final String MIN_SDK_PARAM = "zooming-min-sdk-version";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_SDK =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_TO_GTS_ANIMATION, MIN_SDK_PARAM, DEFAULT_MIN_SDK);

    // Field trial parameter for the minimum physical memory size to enable zooming animation.
    public static final String MIN_MEMORY_MB_PARAM = "zooming-min-memory-mb";
    public static final IntCachedFieldTrialParameter ZOOMING_MIN_MEMORY =
            new IntCachedFieldTrialParameter(ChromeFeatureList.TAB_TO_GTS_ANIMATION,
                    MIN_MEMORY_MB_PARAM, DEFAULT_MIN_MEMORY_MB);

    private static Boolean sIsTabToGtsAnimationEnabled;

    /**
     * Toggles whether the Tab-to-GTS animation is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setIsTabToGtsAnimationEnabledForTesting(@Nullable Boolean enabled) {
        sIsTabToGtsAnimationEnabled = enabled;
    }

    /**
     * @return Whether the Tab-to-Grid (and Grid-to-Tab) transition animation is enabled.
     */
    public static boolean isTabToGtsAnimationEnabled() {
        if (sIsTabToGtsAnimationEnabled != null) {
            Log.d(TAG, "IsTabToGtsAnimationEnabled forced to " + sIsTabToGtsAnimationEnabled);
            return sIsTabToGtsAnimationEnabled;
        }
        Log.d(TAG, "GTS.MinSdkVersion = " + GridTabSwitcherUtil.getMinSdkVersion());
        Log.d(TAG, "GTS.MinMemoryMB = " + GridTabSwitcherUtil.getMinMemoryMB());
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                && Build.VERSION.SDK_INT >= GridTabSwitcherUtil.getMinSdkVersion()
                && SysUtils.amountOfPhysicalMemoryKB() / 1024
                >= GridTabSwitcherUtil.getMinMemoryMB();
    }

    private static class GridTabSwitcherUtil {
        private static int getMinSdkVersion() {
            return TabFeatureUtilities.ZOOMING_MIN_SDK.getValue();
        }

        private static int getMinMemoryMB() {
            return TabFeatureUtilities.ZOOMING_MIN_MEMORY.getValue();
        }
    }
}
