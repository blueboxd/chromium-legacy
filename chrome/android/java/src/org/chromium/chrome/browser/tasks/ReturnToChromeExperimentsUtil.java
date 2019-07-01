// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * This is a utility class for managing experiments related to returning to Chrome.
 */
public final class ReturnToChromeExperimentsUtil {
    @VisibleForTesting
    public static final String TAB_SWITCHER_ON_RETURN_MS = "tab_switcher_on_return_time_ms";

    private ReturnToChromeExperimentsUtil() {}

    /**
     * Determine if we should show the tab switcher on returning to Chrome.
     *   Returns true if enough time has elapsed since the app was last backgrounded.
     *   The threshold time in milliseconds is set by experiment "enable-tab-switcher-on-return"
     *
     * @param lastBackgroundedTimeMillis The last time the application was backgrounded. Set in
     *                                   ChromeTabbedActivity::onStopWithNative
     * @return true if past threshold, false if not past threshold or experiment cannot be loaded.
     */
    public static boolean shouldShowTabSwitcher(final long lastBackgroundedTimeMillis) {
        if (lastBackgroundedTimeMillis == -1) {
            // No last background timestamp set, use control behavior.
            return false;
        }

        int tabSwitcherAfterMillis = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.TAB_SWITCHER_ON_RETURN, TAB_SWITCHER_ON_RETURN_MS, -1);

        if (tabSwitcherAfterMillis < 0) {
            // If no value for experiment, use control behavior.
            return false;
        }

        long expirationTime = lastBackgroundedTimeMillis + tabSwitcherAfterMillis;

        return System.currentTimeMillis() > expirationTime;
    }

    /**
     * Whether we should display the omnibox on the tab switcher in addition to the
     *  tab switcher toolbar.
     *
     * Depends on tab grid being enabled.
     *
     * @return true if the tab switcher on return and tab grid features are both ON, else false.
     */
    public static boolean shouldShowOmniboxOnTabSwitcher() {
        return ChromeFeatureList.isInitialized()
                && (FeatureUtilities.isGridTabSwitcherEnabled()
                        || FeatureUtilities.isTabGroupsAndroidEnabled())
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_SWITCHER_ON_RETURN);
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param url The URL to load.
     * @param transition The page transition type.
     * @return true if we have handled the navigation, false otherwise.
     */
    public static boolean willHandleLoadUrlFromLocationBar(
            String url, @PageTransition int transition) {
        ChromeActivity chromeActivity = getActivityPresentingOverviewWithOmnibox();
        if (chromeActivity == null) return false;

        // Create a new unparented tab.
        TabModel model = chromeActivity.getCurrentTabModel();
        LoadUrlParams params = new LoadUrlParams(url);
        params.setTransitionType(transition | PageTransition.FROM_ADDRESS_BAR);
        chromeActivity.getTabCreator(model.isIncognito())
                .createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);

        RecordUserAction.record("MobileOmniboxUse.GridTabSwitcher");

        // These are duplicated here but would have been recorded by LocationBarLayout#loadUrl.
        RecordUserAction.record("MobileOmniboxUse");
        LocaleManager.getInstance().recordLocaleBasedSearchMetrics(false, url, transition);

        return true;
    }

    /**
     * @return Whether the Tab Switcher is showing the omnibox.
     */
    public static boolean isInOverviewWithOmnibox() {
        return getActivityPresentingOverviewWithOmnibox() != null;
    }

    /**
     * @return The ChromeActivity if it is presenting the omnibox on the tab switcher, else null.
     */
    private static ChromeActivity getActivityPresentingOverviewWithOmnibox() {
        if (!shouldShowOmniboxOnTabSwitcher()) return null;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return null;

        ChromeActivity chromeActivity = (ChromeActivity) activity;
        if (!chromeActivity.isInOverviewMode()) return null;

        return chromeActivity;
    }
}
