// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * A class to handle the state of flags for tab_management.
 */
public class TabUiFeatureUtilities {
    private static Boolean sSearchTermChipEnabledForTesting;
    private static Boolean sTabManagementModuleSupportedForTesting;

    /**
     * Set whether the search term chip in Grid tab switcher is enabled for testing.
     */
    public static void setSearchTermChipEnabledForTesting(@Nullable Boolean enabled) {
        sSearchTermChipEnabledForTesting = enabled;
    }

    /**
     * @return Whether the search term chip in Grid tab switcher is enabled.
     */
    public static boolean isSearchTermChipEnabled() {
        if (sSearchTermChipEnabledForTesting != null) return sSearchTermChipEnabledForTesting;
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, "enable_search_term_chip", false);
    }

    /**
     * Set whether the tab management module is supported for testing.
     */
    public static void setTabManagementModuleSupportedForTesting(@Nullable Boolean enabled) {
        sTabManagementModuleSupportedForTesting = enabled;
    }

    /**
     * @return Whether the tab management module is supported.
     */
    private static boolean isTabManagementModuleSupported() {
        if (sTabManagementModuleSupportedForTesting != null) {
            return sTabManagementModuleSupportedForTesting;
        }

        return TabManagementModuleProvider.isTabManagementModuleSupported();
    }

    /**
     * @return Tab UI related feature flags that should be cached.
     */
    public static List<String> getFeaturesToCache() {
        if (!isEligibleForTabUiExperiments() || DeviceClassManager.enableAccessibilityLayout()) {
            return Collections.emptyList();
        }
        return Arrays.asList(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                ChromeFeatureList.TAB_GROUPS_ANDROID,
                ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID);
    }

    private static boolean isEligibleForTabUiExperiments() {
        return (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
                       || !SysUtils.isLowEndDevice())
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    /**
     * @return Whether the Grid Tab Switcher UI is enabled and available for use.
     */
    public static boolean isGridTabSwitcherEnabled() {
        // TODO(yusufo): AccessibilityLayout check should not be here and the flow should support
        // changing that setting while Chrome is alive.
        // Having Tab Groups or Start implies Grid Tab Switcher.
        return !(ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
                       && SysUtils.isLowEndDevice())
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID)
                && isTabManagementModuleSupported()
                || isTabGroupsAndroidEnabled() || CachedFeatureFlags.isStartSurfaceEnabled();
    }

    /**
     * @return Whether the tab group feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)
                && isTabManagementModuleSupported();
    }

    /**
     * @return Whether the tab strip and duet integration feature is enabled and available for use.
     */
    public static boolean isDuetTabStripIntegrationAndroidEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TAB_GROUPS_ANDROID)
                && CachedFeatureFlags.isEnabled(ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID)
                && isTabManagementModuleSupported();
    }

    /**
     * @return Whether the tab group ui improvement feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidUiImprovementsEnabled() {
        return isTabGroupsAndroidEnabled()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID);
    }

    /**
     * @return Whether the tab group continuation feature is enabled and available for use.
     */
    public static boolean isTabGroupsAndroidContinuationEnabled() {
        return isTabGroupsAndroidEnabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID);
    }
}
