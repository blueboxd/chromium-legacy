// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Allows querying bottom toolbar related configurations.
 */
public class BottomToolbarConfiguration {
    /**
     * @return Whether or not the bottom toolbar is enabled.
     */
    public static boolean isBottomToolbarEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CHROME_DUET)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext())
                && (TabUiFeatureUtilities.isDuetTabStripIntegrationAndroidEnabled()
                        || !TabUiFeatureUtilities.isTabGroupsAndroidEnabled());
    }

    /**
     * @return Whether or not the adaptive toolbar is enabled.
     */
    public static boolean isAdaptiveToolbarEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CHROME_DUET_ADAPTIVE)
                && isBottomToolbarEnabled();
    }

    /**
     * @return Whether or not the labeled bottom toolbar is enabled.
     */
    public static boolean isLabeledBottomToolbarEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CHROME_DUET_LABELED)
                && isBottomToolbarEnabled();
    }
}
