// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A class to handle the state of flags for tab_management.
 */
public class TabUiFeatureUtilities {
    private static Boolean sSearchTermChipEnabledForTesting;

    /**
     * Set whether the search term chip in Grid tab switcher is enabled for testing.
     */
    public static void setSearchTermChipEnabledForTesting(Boolean enabled) {
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
}
