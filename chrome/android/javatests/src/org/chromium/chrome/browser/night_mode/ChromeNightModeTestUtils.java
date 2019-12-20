// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.themes.ThemeType;

/**
 * Helper methods to be used in tests to specify night mode state. See also {@link
 * org.chromium.ui.test.util.NightModeTestUtils}.
 */
public class ChromeNightModeTestUtils {
    /**
     * Sets up initial states for night mode before
     * {@link org.chromium.chrome.browser.ChromeActivity} is launched.
     */
    public static void setUpNightModeBeforeChromeActivityLaunched() {
        FeatureUtilities.setNightModeAvailableForTesting(true);
        NightModeUtils.setNightModeSupportedForTesting(true);
    }

    /**
     * Sets up the night mode state for {@link org.chromium.chrome.browser.ChromeActivity}.
     * @param nightModeEnabled Whether night mode should be enabled.
     */
    public static void setUpNightModeForChromeActivity(boolean nightModeEnabled) {
        SharedPreferencesManager.getInstance().writeInt(ChromePreferenceKeys.UI_THEME_SETTING,
                nightModeEnabled ? ThemeType.DARK : ThemeType.LIGHT);
    }

    /**
     * Resets the night mode state after {@link org.chromium.chrome.browser.ChromeActivity} is
     * destroyed.
     */
    public static void tearDownNightModeAfterChromeActivityDestroyed() {
        FeatureUtilities.setNightModeAvailableForTesting(null);
        NightModeUtils.setNightModeSupportedForTesting(null);
        GlobalNightModeStateProviderHolder.resetInstanceForTesting();
        SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.UI_THEME_SETTING);
    }
}
