// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.TopToolbarThemeColorProvider;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.chrome.browser.webapps.WebappExtras;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Maintains the toolbar color for {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabToolbarColorController extends TopToolbarThemeColorProvider {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;

    private ToolbarManager mToolbarManager;
    private boolean mUseTabThemeColor;

    @Inject
    public CustomTabToolbarColorController(@Named(ACTIVITY_CONTEXT) Context activityContext,
            ActivityTabProvider tabProvider, TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider) {
        super(activityContext, tabProvider);
        mTabObserverRegistrar = tabObserverRegistrar;
        mIntentDataProvider = intentDataProvider;
    }

    /**
     * Notifies the controller that the ToolbarManager has been created and is ready for use.
     * ToolbarManager isn't passed directly to the constructor because it's not guaranteed to be
     * initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        mToolbarManager = manager;
        assert manager != null : "Toolbar manager not initialized";

        observeTabToUpdateColor();

        updateTheme(false /* shouldAnimate */);
    }

    private void observeTabToUpdateColor() {
        mTabObserverRegistrar.registerActivityTabObserver(new CustomTabTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Update the color when the page load finishes.
                updateTheme(false /* shouldAnimate */);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the color on every new URL.
                updateTheme(false /* shouldAnimate */);
            }

            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                updateTheme(false /* shouldAnimate */);
            }
        });
    }

    /**
     * Sets whether the tab's theme color should be used for the toolbar and triggers an update of
     * the toolbar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;
        updateTheme(false /* shouldAnimate */);
    }

    @Override
    protected void updateTheme(boolean shouldAnimate) {
        super.updateTheme(shouldAnimate);

        if (mToolbarManager != null) {
            mToolbarManager.setShouldUpdateToolbarPrimaryColor(true);
            mToolbarManager.onThemeColorChanged(getThemeColor(), false);
            mToolbarManager.setShouldUpdateToolbarPrimaryColor(false);
        }
    }

    @Override
    protected @ColorInt int computeThemeColor(Tab activeTab) {
        if (mIntentDataProvider.isOpenedByChrome()) {
            return super.computeThemeColor(activeTab);
        }

        if (shouldUseDefaultThemeColorForFullscreen(mIntentDataProvider)
                || (activeTab != null && TabThemeColorHelper.isDefaultColorUsed(activeTab))) {
            return DEFAULT_THEME_COLOR;
        }

        if (activeTab != null && mUseTabThemeColor) {
            return TabThemeColorHelper.getColor(activeTab);
        }

        return mIntentDataProvider.hasCustomToolbarColor() ? mIntentDataProvider.getToolbarColor()
                                                           : DEFAULT_THEME_COLOR;
    }

    private static boolean shouldUseDefaultThemeColorForFullscreen(
            BrowserServicesIntentDataProvider intentDataProvider) {
        // Don't use the theme color provided by the page if we're in display: fullscreen. This
        // works around an issue where the status bars go transparent and can't be seen on top of
        // the page content when users swipe them in or they appear because the on-screen keyboard
        // was triggered.
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        return (webappExtras != null && webappExtras.displayMode == WebDisplayMode.FULLSCREEN);
    }
}
