// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends NewTabPage {
    /**
     * Constructs a new {@link FeedNewTabPage}.
     *
     * @param activity The containing {@link ChromeActivity}.
     * @param nativePageHost The host for this native page.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param activityTabProvider Allows us to check if we are the current tab.
     * @param activityLifecycleDispatcher Allows us to subscribe to backgrounding events.
     * @param tab The {@link TabImpl} that contains this new tab page.
     */
    public FeedNewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector, ActivityTabProvider activityTabProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher, TabImpl tab) {
        super(activity, nativePageHost, tabModelSelector, activityTabProvider,
                activityLifecycleDispatcher, tab);
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return true;
    }
}
