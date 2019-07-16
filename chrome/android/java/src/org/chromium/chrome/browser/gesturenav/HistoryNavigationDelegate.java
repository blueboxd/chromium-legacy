// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Provides navigation-related configuration for pages using {@link HistoryNavigationLayout}.
 */
public abstract class HistoryNavigationDelegate {
    private final boolean mIsEnabled;

    private HistoryNavigationDelegate(Context context) {
        mIsEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION)
                && (context instanceof ChromeActivity);
    }

    /**
     * @return {@code true} if history navigation is enabled.
     */
    public boolean isEnabled() {
        return mIsEnabled;
    }

    /**
     * @return {@link NavigationHandler#ActionDelegate} object.
     */
    public abstract NavigationHandler.ActionDelegate createActionDelegate();

    // Implementation for native pages with TabbedActionDelegate that uses Tab.goForward/goBack
    // to implement history navigation.
    private static class NativePageDelegate extends HistoryNavigationDelegate {
        private final Tab mTab;

        private NativePageDelegate(Tab tab) {
            super(tab.getActivity());
            mTab = tab;
        }

        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return new TabbedActionDelegate(mTab);
        }
    }

    /**
     * Creates {@link HistoryNavigationDelegate} for a native page.
     */
    public static HistoryNavigationDelegate createForNativePage(Tab tab) {
        return new NativePageDelegate(tab);
    }
}
