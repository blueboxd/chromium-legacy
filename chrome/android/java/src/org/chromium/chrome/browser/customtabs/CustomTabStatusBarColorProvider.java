// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.chrome.browser.webapps.WebappExtras;

import javax.inject.Inject;

/**
 * Manages the status bar color for a CustomTabActivity.
 */
@ActivityScope
public class CustomTabStatusBarColorProvider {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ActivityTabProvider mActivityTabProvider;
    private final StatusBarColorController mStatusBarColorController;

    private boolean mUseTabThemeColor;

    @Inject
    public CustomTabStatusBarColorProvider(BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider,
            StatusBarColorController statusBarColorController) {
        mIntentDataProvider = intentDataProvider;
        mActivityTabProvider = activityTabProvider;
        mStatusBarColorController = statusBarColorController;
    }

    /**
     * Sets whether the tab's theme color should be used for the status bar and triggers an update
     * of the status bar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;
        mStatusBarColorController.updateStatusBarColor();
    }

    int getBaseStatusBarColor(int fallbackStatusBarColor) {
        if (mIntentDataProvider.isOpenedByChrome()) return fallbackStatusBarColor;

        Tab tab = mActivityTabProvider.get();
        if (tab == null) {
            return mIntentDataProvider.getToolbarColor();
        }

        if (shouldUseDefaultThemeColorForFullscreen()) {
            return TabThemeColorHelper.getDefaultColor(tab);
        }

        return mUseTabThemeColor ? UNDEFINED_STATUS_BAR_COLOR
                                 : mIntentDataProvider.getToolbarColor();
    }

    boolean isStatusBarDefaultThemeColor(boolean isFallbackColorDefault) {
        if (mIntentDataProvider.isOpenedByChrome()) {
            return isFallbackColorDefault;
        }

        return shouldUseDefaultThemeColorForFullscreen();
    }

    private boolean shouldUseDefaultThemeColorForFullscreen() {
        // Don't use the theme color provided by the page if we're in display: fullscreen. This
        // works around an issue where the status bars go transparent and can't be seen on top of
        // the page content when users swipe them in or they appear because the on-screen keyboard
        // was triggered.
        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        return (webappExtras != null && webappExtras.displayMode == WebDisplayMode.FULLSCREEN);
    }
}
