// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.graphics.Color;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.FeatureHighlightProvider;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the browsing mode bottom
 * toolbar, and updating the model accordingly.
 */
class BrowsingModeBottomToolbarMediator implements ThemeColorObserver {
    /** The transparency fraction of the IPH bubble. */
    private static final float DUET_IPH_BUBBLE_ALPHA_FRACTION = 0.9f;

    /** The transparency fraction of the IPH background. */
    private static final float DUET_IPH_BACKGROUND_ALPHA_FRACTION = 0.3f;

    /** The dismissable parameter name of the IPH. */
    static final String DUET_IPH_TAP_TO_DISMISS_PARAM_NAME = "duet_iph_tap_to_dismiss_enabled";

    /** The model for the browsing mode bottom toolbar that holds all of its state. */
    private final BrowsingModeBottomToolbarModel mModel;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    private FeatureHighlightProvider mFeatureHighlightProvider;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BrowsingModeBottomToolbarModel} that holds all the state for the
     *              browsing mode  bottom toolbar.
     */
    BrowsingModeBottomToolbarMediator(BrowsingModeBottomToolbarModel model) {
        mModel = model;
        mFeatureHighlightProvider = AppHooks.get().createFeatureHighlightProvider();
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addThemeColorObserver(this);
    }

    /**
     * Maybe show the IPH bubble for Chrome Duet.
     * @param feature A String identifying the feature.
     * @param activity An activity to attach the IPH to.
     * @param anchor The view to anchor the IPH to.
     * @param tracker A tracker for IPH.
     */
    void showIPH(@FeatureConstants String feature, ChromeActivity activity, View anchor,
            Tracker tracker) {
        if (!tracker.shouldTriggerHelpUI(feature) || !anchor.isShown() || !anchor.isEnabled()) {
            return;
        }
        int innerBackgroundColor =
                ApiCompatibilityUtils.getColor(anchor.getResources(), R.color.default_bg_color);
        int baseBubbleColor =
                ApiCompatibilityUtils.getColor(anchor.getResources(), R.color.modern_blue_600);

        // Clear out the alpha and use custom transparency.
        int finalOuterColor =
                applyCustomAlphaToColor(baseBubbleColor, DUET_IPH_BUBBLE_ALPHA_FRACTION);
        int finalScrimColor =
                applyCustomAlphaToColor(baseBubbleColor, DUET_IPH_BACKGROUND_ALPHA_FRACTION);

        @StringRes
        int titleId = 0;
        @StringRes
        int descId = 0;
        switch (feature) {
            case FeatureConstants.CHROME_DUET_HOME_BUTTON_FEATURE:
                titleId = R.string.iph_duet_home_button_title;
                descId = R.string.iph_duet_home_button_description;
                break;
            case FeatureConstants.CHROME_DUET_SEARCH_FEATURE:
                titleId = R.string.iph_duet_search_title;
                descId = R.string.iph_duet_search_description;
                break;
            case FeatureConstants.CHROME_DUET_TAB_SWITCHER_FEATURE:
                titleId = R.string.iph_duet_tab_switcher_title;
                descId = R.string.iph_duet_tab_switcher_description;
                break;
            default:
                assert false : "Unsupported FeatureConstants: " + feature;
        }

        // Default value for whether to able to dismiss the IPH for duet is true.
        boolean tapToDismiss = true;
        if (ChromeFeatureList.isInitialized()) {
            tapToDismiss = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    feature, DUET_IPH_TAP_TO_DISMISS_PARAM_NAME, true);
        }

        if (tapToDismiss) {
            // When users can dismiss the IPH, the outer background should be total transparent.
            finalScrimColor = Color.TRANSPARENT;
        }

        mFeatureHighlightProvider.buildForView(activity, anchor, titleId,
                FeatureHighlightProvider.TextAlignment.CENTER,
                R.style.TextAppearance_TextLarge_Primary_Light, descId,
                FeatureHighlightProvider.TextAlignment.CENTER,
                R.style.TextAppearance_TextMedium_Primary_Light, innerBackgroundColor,
                finalOuterColor, finalScrimColor, FeatureHighlightProvider.NO_TIMEOUT,
                tapToDismiss);
    }

    /**
     * Dismiss the IPH bubble for Chrome Duet.
     * @param activity An activity to attach the IPH to.
     */
    void dismissIPH(AppCompatActivity activity) {
        mFeatureHighlightProvider.dismiss(activity);
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeThemeColorObserver(this);
            mThemeColorProvider = null;
        }
    }

    @Override
    public void onThemeColorChanged(int primaryColor, boolean shouldAnimate) {
        mModel.set(BrowsingModeBottomToolbarModel.PRIMARY_COLOR, primaryColor);
    }

    /**
     * Set the alpha for the color.
     * @param baseColor The color which alpha will apply to.
     * @param alpha The desired alpha for the color. The value should between 0 to 1. 0 means total
     *         transparency, 1 means total non-transparency.
     */
    private @ColorInt int applyCustomAlphaToColor(@ColorInt int baseColor, float alpha) {
        return (baseColor & 0x00FFFFFF) | ((int) (alpha * 255) << 24);
    }
}
