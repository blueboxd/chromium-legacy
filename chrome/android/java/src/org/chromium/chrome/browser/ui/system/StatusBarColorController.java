// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarThemeColorProvider;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/**
 * Maintains the status bar color for a {@link ChromeActivity}.
 */
public class StatusBarColorController implements Destroyable,
                                                 TopToolbarCoordinator.UrlExpansionObserver,
                                                 StatusIndicatorCoordinator.StatusIndicatorObserver,
                                                 ThemeColorProvider.ThemeColorObserver {
    public static final @ColorInt int UNDEFINED_STATUS_BAR_COLOR = Color.TRANSPARENT;

    private final Window mWindow;
    private final boolean mIsTablet;
    private final ActivityTabProvider mTabProvider;
    private final @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private final ScrimView.StatusBarScrimDelegate mStatusBarScrimDelegate;
    private final ActivityTabProvider.ActivityTabTabObserver mStatusBarColorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    private final @ColorInt int mStandardPrimaryBgColor;
    private final @ColorInt int mIncognitoPrimaryBgColor;
    private final @ColorInt int mStandardDefaultThemeColor;
    private final @ColorInt int mIncognitoDefaultThemeColor;

    /** Provides top toolbar theme color. */
    private @Nullable TopToolbarThemeColorProvider mTopToolbarThemeColorProvider;

    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private boolean mIsInOverviewMode;
    private boolean mIsIncognito;

    private @ColorInt int mScrimColor;
    private float mStatusBarScrimFraction;

    private float mToolbarUrlExpansionPercentage;
    private boolean mShouldUpdateStatusBarColorForNTP;
    private @ColorInt int mStatusIndicatorColor;
    private @ColorInt int mStatusBarColorWithoutStatusIndicator;

    /**
     * @param chromeActivity The {@link ChromeActivity} that this class is attached to.
     * @param topToolbarThemeColorProvider The {@link TopToolbarThemeColorProvider} to observe. The
     *         status bar color is generally based on the top toolbar theme color when in browsing
     *         mode.
     */
    public StatusBarColorController(ChromeActivity chromeActivity,
            TopToolbarThemeColorProvider topToolbarThemeColorProvider) {
        mWindow = chromeActivity.getWindow();
        mIsTablet = chromeActivity.isTablet();
        mTabProvider = chromeActivity.getActivityTabProvider();
        mOverviewModeBehavior = chromeActivity.getOverviewModeBehavior();
        mStatusBarScrimDelegate = (fraction) -> {
            mStatusBarScrimFraction = fraction;
            updateStatusBarColor();
        };

        Resources resources = chromeActivity.getResources();
        mStandardPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, false);
        mIncognitoPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, true);
        mStandardDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, false);
        mIncognitoDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, true);

        mStatusIndicatorColor = UNDEFINED_STATUS_BAR_COLOR;

        mStatusBarColorTabObserver = new ActivityTabProvider.ActivityTabTabObserver(mTabProvider) {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                updateStatusBarColor();
            }

            @Override
            public void onContentChanged(Tab tab) {
                final boolean newShouldUpdateStatusBarColorForNTP = isStandardNTP();
                // Also update the status bar color if the content was previously an NTP, because an
                // NTP can use a different status bar color than the default theme color. In this
                // case, the theme color might not change, and thus #onDidChangeThemeColor might
                // not get called.
                if (mShouldUpdateStatusBarColorForNTP || newShouldUpdateStatusBarColorForNTP) {
                    updateStatusBarColor();
                }
                mShouldUpdateStatusBarColorForNTP = newShouldUpdateStatusBarColorForNTP;
            }

            @Override
            public void onDestroyed(Tab tab) {
                // Make sure that #mShouldUpdateStatusBarColorForNTP is cleared because
                // #onObservingDifferentTab() might not be notified early enough when
                // #onUrlExpansionPercentageChanged() is called.
                mShouldUpdateStatusBarColorForNTP = false;
            }

            @Override
            protected void onObservingDifferentTab(Tab tab) {
                final boolean oldShouldUpdateStatusBarColorForNTP =
                        mShouldUpdateStatusBarColorForNTP;
                mShouldUpdateStatusBarColorForNTP = isStandardNTP();

                // Recompute status bar color if switching between an NTP and non-NTP tab.
                if (mShouldUpdateStatusBarColorForNTP != oldShouldUpdateStatusBarColorForNTP) {
                    updateStatusBarColor();
                }
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mIsIncognito = newModel.isIncognito();

                // When opening a new Incognito Tab from a normal Tab (or vice versa), the
                // status bar color is updated. However, this update is triggered after the
                // animation, so we update here for the duration of the new Tab animation.
                // See https://crbug.com/917689.
                updateStatusBarColor();
            }
        };

        if (mOverviewModeBehavior != null) {
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    mIsInOverviewMode = true;
                    updateStatusBarColor();
                }

                @Override
                public void onOverviewModeFinishedHiding() {
                    mIsInOverviewMode = false;
                    updateStatusBarColor();
                }
            };
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }

        chromeActivity.getLifecycleDispatcher().register(this);

        mTopToolbarThemeColorProvider = topToolbarThemeColorProvider;
        mTopToolbarThemeColorProvider.addThemeColorObserver(this);
    }

    // ThemeColorProvider.ThemeColorObserver implementation.
    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        updateStatusBarColor();
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        mStatusBarColorTabObserver.destroy();
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        mTopToolbarThemeColorProvider.removeThemeColorObserver(this);
    }

    // TopToolbarCoordinator.UrlExpansionObserver implementation.
    @Override
    public void onUrlExpansionPercentageChanged(float percentage) {
        mToolbarUrlExpansionPercentage = percentage;
        if (mShouldUpdateStatusBarColorForNTP) updateStatusBarColor();
    }

    // StatusIndicatorCoordinator.StatusIndicatorObserver implementation.

    @Override
    public void onStatusIndicatorColorChanged(@ColorInt int newColor) {
        mStatusIndicatorColor = newColor;
        updateStatusBarColor();
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} to check whether incognito model is
     *                         selected.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert mTabModelSelector == null : "mTabModelSelector should only be set once.";
        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) {
            mTabModelSelector.addObserver(mTabModelSelectorObserver);
            updateStatusBarColor();
        }
    }

    /**
     * @return The {@link ScrimView.StatusBarScrimDelegate} that adjusts the status bar color based
     *         on the scrim offset.
     */
    public ScrimView.StatusBarScrimDelegate getStatusBarScrimDelegate() {
        return mStatusBarScrimDelegate;
    }

    public void updateStatusBarColor() {
        mStatusBarColorWithoutStatusIndicator = calculateStatusBarColor();
        int statusBarColor = mStatusBarColorWithoutStatusIndicator;

        final boolean statusIndicatorColorSet = mStatusIndicatorColor != UNDEFINED_STATUS_BAR_COLOR;
        if (statusIndicatorColorSet) {
            statusBarColor = mStatusIndicatorColor;
        }

        // If the API level is not at least M, the status bar icons will be always light. So, we
        // should darken the status bar color.
        boolean shouldDarkenStatusBar = Build.VERSION.SDK_INT < Build.VERSION_CODES.M;
        if (!shouldDarkenStatusBar) {
            // If we aren't darkening the color, we should apply scrim if it's showing.
            statusBarColor = applyCurrentScrimToColor(statusBarColor);
        }

        setStatusBarColor(statusBarColor);
    }

    // TODO(sinansahin): Confirm pre-M expectations with UX and update as needed.
    /**
     * @return The status bar color without the status indicator's color taken into consideration.
     *         Color returned from this method includes darkening if the OS version doesn't support
     *         light status bar icons (pre-M). However, scrimming isn't included since it's managed
     *         completely by this class.
     */
    public @ColorInt int getStatusBarColorWithoutStatusIndicator() {
        return mStatusBarColorWithoutStatusIndicator;
    }

    private @ColorInt int calculateStatusBarColor() {
        // We don't adjust status bar color for tablet.
        if (mIsTablet) return Color.BLACK;

        // Return status bar color in overview mode.
        boolean supportsDarkStatusIcons = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
        if (mIsInOverviewMode) {
            if (!supportsDarkStatusIcons) return Color.BLACK;

            return (mIsIncognito && ToolbarColors.canUseIncognitoToolbarThemeColorInOverview())
                    ? mIncognitoPrimaryBgColor
                    : mStandardPrimaryBgColor;
        }

        // Return status bar color in standard NewTabPage. If location bar is not shown in NTP, we
        // use the tab theme color regardless of the URL expansion percentage.
        if (isLocationBarShownInNTP()) {
            if (!supportsDarkStatusIcons) return Color.BLACK;

            return ColorUtils.getColorWithOverlay(
                    TabThemeColorHelper.getBackgroundColor(mTabProvider.get()),
                    mTopToolbarThemeColorProvider.getThemeColor(), mToolbarUrlExpansionPercentage);
        }

        if (!supportsDarkStatusIcons && mTopToolbarThemeColorProvider.isDefaultColorUsed()) {
            return Color.BLACK;
        }
        return mTopToolbarThemeColorProvider.getThemeColor();
    }

    /**
     * Set device status bar to a given color. Also, set the status bar icons to a dark color if
     * needed.
     * @param color The color that the status bar should be set to.
     */
    private void setStatusBarColor(@ColorInt int color) {
        if (UiUtils.isSystemUiThemingDisabled()) return;

        final View root = mWindow.getDecorView().getRootView();
        boolean needsDarkStatusBarIcons = !ColorUtils.shouldUseLightForegroundOnBackground(color);
        ApiCompatibilityUtils.setStatusBarIconColor(root, needsDarkStatusBarIcons);
        ApiCompatibilityUtils.setStatusBarColor(mWindow, color);
    }

    /**
     * Get the scrim applied color if the scrim is showing. Otherwise, return the original color.
     * @param color Color to maybe apply scrim to.
     * @return The resulting color.
     */
    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        if (mScrimColor == 0) {
            final View root = mWindow.getDecorView().getRootView();
            final Resources resources = root.getResources();
            mScrimColor = ApiCompatibilityUtils.getColor(resources, R.color.black_alpha_65);
        }
        // Apply a color overlay if the scrim is showing.
        float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
        int scrimColorOpaque = mScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(
                color, scrimColorOpaque, mStatusBarScrimFraction * scrimColorAlpha);
    }

    /**
     * @return Whether or not the current tab is a new tab page in standard mode.
     */
    private boolean isStandardNTP() {
        return mTabProvider.get() != null
                && mTabProvider.get().getNativePage() instanceof NewTabPage;
    }

    /**
     * @return Whether or not the fake location bar is shown on the current NTP.
     */
    private boolean isLocationBarShownInNTP() {
        if (!isStandardNTP()) return false;
        final NewTabPage newTabPage = (NewTabPage) mTabProvider.get().getNativePage();
        return newTabPage != null && newTabPage.isLocationBarShownInNTP();
    }
}
