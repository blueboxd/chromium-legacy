// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends NewTabPage {
    /**
     * Constructs a new {@link FeedNewTabPage}.
     *
     * @param activity The containing {@link Activity}.
     * @param fullscreenManager {@link ChromeFullscreenManager} to observe for offset changes.
     * @param activityTabProvider Provides the current active tab.
     * @param overviewModeBehavior Overview mode to observe for mode changes.
     * @param snackbarManager {@link SnackbarManager} object.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param isTablet {@code true} if running on a Tablet device.
     * @param uma {@link NewTabPageUma} object recording user metrics.
     * @param isInNightMode {@code true} if the night mode setting is on.
     * @param nativePageHost The host for this native page.
     * @param tab The {@link Tab} that contains this new tab page.
     */
    public FeedNewTabPage(Activity activity, ChromeFullscreenManager fullscreenManager,
                          Supplier<Tab> activityTabProvider, @Nullable OverviewModeBehavior overviewModeBehavior,
                          SnackbarManager snackbarManager, ActivityLifecycleDispatcher lifecycleDispatcher,
                          TabModelSelector tabModelSelector, boolean isTablet, NewTabPageUma uma,
                          boolean isInNightMode, NativePageHost nativePageHost, Tab tab) {
        super(activity, fullscreenManager, activityTabProvider, overviewModeBehavior,
                snackbarManager, lifecycleDispatcher, tabModelSelector, isTablet, uma,
                isInNightMode, nativePageHost, tab);
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return true;
    }
}
