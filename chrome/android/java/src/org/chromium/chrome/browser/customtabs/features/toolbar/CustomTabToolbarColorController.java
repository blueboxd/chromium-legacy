// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.styles.ChromeColors;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Maintains the toolbar color for {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabToolbarColorController implements InflationObserver {
    private final Lazy<ToolbarManager> mToolbarManager;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ChromeActivity mActivity;
    private final TabObserverRegistrar mTabObserverRegistrar;

    @Inject
    public CustomTabToolbarColorController(ActivityLifecycleDispatcher lifecycleDispatcher,
            Lazy<ToolbarManager> toolbarManager,
            BrowserServicesIntentDataProvider intentDataProvider,
            ChromeActivity activity,
            TabObserverRegistrar tabObserverRegistrar) {
        mToolbarManager = toolbarManager;
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mTabObserverRegistrar = tabObserverRegistrar;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        ToolbarManager manager = mToolbarManager.get();
        assert manager != null : "Toolbar manager not initialized";

        int toolbarColor = mIntentDataProvider.getToolbarColor();
        manager.onThemeColorChanged(toolbarColor, false);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            manager.setShouldUpdateToolbarPrimaryColor(false);
        }
        observeTabToUpdateColor();
    }

    private void observeTabToUpdateColor() {
        mTabObserverRegistrar.registerTabObserver(new EmptyTabObserver() {
            /** Keeps track of the original color before the preview was shown. */
            private int mOriginalColor;

            /** True if a change to the toolbar color was made because of a preview. */
            private boolean mTriggeredPreviewChange;

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Update the color when the page load finishes.
                updateColor(tab);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the color on every new URL.
                updateColor(tab);
            }

            /**
             * Updates the color of the Activity's CCT Toolbar. When a preview is shown, it should
             * be reset to the default color. If the user later navigates away from that preview to
             * a non-preview page, reset the color back to the original. This does not interfere
             * with site-specific theme colors which are disabled when a preview is being shown.
             */
            private void updateColor(Tab tab) {
                ToolbarManager manager = mToolbarManager.get();

                // Record the original toolbar color in case we need to revert back to it later
                // after a preview has been shown then the user navigates to another non-preview
                // page.
                if (mOriginalColor == 0) mOriginalColor = manager.getPrimaryColor();

                final boolean shouldUpdateOriginal = manager.getShouldUpdateToolbarPrimaryColor();
                manager.setShouldUpdateToolbarPrimaryColor(true);

                if (tab.isPreview()) {
                    final int defaultColor =
                            ChromeColors.getDefaultThemeColor(mActivity.getResources(), false);
                    manager.onThemeColorChanged(defaultColor, false);
                    mTriggeredPreviewChange = true;
                } else if (mOriginalColor != manager.getPrimaryColor() && mTriggeredPreviewChange) {
                    manager.onThemeColorChanged(mOriginalColor, false);
                    mTriggeredPreviewChange = false;
                    mOriginalColor = 0;
                }

                manager.setShouldUpdateToolbarPrimaryColor(shouldUpdateOriginal);
            }
        });
    }
}
