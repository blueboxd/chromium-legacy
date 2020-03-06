// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** ThemeColorProvider for the top toolbar. */
public class TopToolbarThemeColorProvider extends ThemeColorProvider {
    /**
     * Color returned by {@link #computeThemeColor()} to indicate that the default theme color
     * should be used.
     */
    protected static final int DEFAULT_THEME_COLOR = Color.TRANSPARENT;

    /** Primary color for standard mode. */
    private final int mStandardPrimaryColor;

    /** Primary color for incognito mode. */
    private final int mIncognitoPrimaryColor;

    /** The {@link sActivityTabTabObserver} used to know when the active tab color changed. */
    private ActivityTabTabObserver mActivityTabTabObserver;

    /** Active tab. */
    private @Nullable Tab mActiveTab;

    /**
     * Returns whether {@link ThemeColorProvider#getThemeColor()} provides the default theme
     * color.
     */
    private boolean mIsDefaultThemeColor;

    public TopToolbarThemeColorProvider(Context context, ActivityTabProvider tabProvider) {
        super(context);

        mStandardPrimaryColor = ChromeColors.getDefaultThemeColor(context.getResources(), false);
        mIncognitoPrimaryColor = ChromeColors.getDefaultThemeColor(context.getResources(), true);

        mActiveTab = tabProvider.get();
        mActivityTabTabObserver = new ActivityTabTabObserver(tabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab) {
                mActiveTab = tab;

                // The ActivityTabProvider active tab is null when in overview. Disable theme
                // color updates while in overview.
                if (tab == null) return;

                updateTheme(false /* shouldAnimate */);
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateTheme(true /* shouldAnimate */);
            }
        };
    }

    /**
     * Returns whether {@link ThemeColorProvider#getThemeColor()} provides the default theme color.
     */
    public boolean isDefaultColorUsed() {
        return mIsDefaultThemeColor;
    }

    /**
     * Updates the toolbar theme color.
     */
    public void updateThemeColor() {
        updateTheme(false /* shouldAnimate */);
    }

    /**
     * Updates the toolbar theme color.
     * @param shouldAnimate Whether the change of color should be animated.
     */
    protected void updateTheme(boolean shouldAnimate) {
        int themeColor = computeThemeColor(mActiveTab);
        mIsDefaultThemeColor = (themeColor == DEFAULT_THEME_COLOR);
        if (mIsDefaultThemeColor) {
            themeColor = (mActiveTab != null && mActiveTab.isIncognito()) ? mIncognitoPrimaryColor
                                                                          : mStandardPrimaryColor;
        }
        updatePrimaryColor(themeColor, shouldAnimate);
    }

    /**
     * Computes the toolbar theme color for the passed-in tab.
     */
    protected @ColorInt int computeThemeColor(@Nullable Tab activeTab) {
        if (activeTab == null || TabThemeColorHelper.isDefaultColorUsed(activeTab)) {
            return DEFAULT_THEME_COLOR;
        }
        return TabThemeColorHelper.getColor(activeTab);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
    }
}
