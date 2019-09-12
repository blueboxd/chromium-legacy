// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarCommonPropertiesModel;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the controller logic of the Status component.
 */
class StatusMediator {
    // The size that the icon should be displayed as.
    private static final int SEARCH_ENGINE_LOGO_ICON_TARGET_SIZE_DP = 24;
    // The size given to LargeIconBridge to increase the probability that we'll get an icon back.
    private static final int SEARCH_ENGINE_LOGO_ICON_DOWNLOAD_SIZE_DP = 16;

    private final PropertyModel mModel;
    private boolean mDarkTheme;
    private boolean mUrlHasFocus;
    private boolean mFirstSuggestionIsSearchQuery;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPreview;
    private boolean mPageIsOffline;
    private boolean mShowStatusIconWhenUrlFocused;
    private boolean mIsSecurityButtonShown;
    private boolean mIsSearchEngineGoogle;
    private boolean mShouldCancelCustomFavicon;

    private int mUrlMinWidth;
    private int mSeparatorMinWidth;
    private int mVerboseStatusTextMinWidth;

    private @ConnectionSecurityLevel int mPageSecurityLevel;

    private @DrawableRes int mSecurityIconRes;
    private @DrawableRes int mSecurityIconTintRes;
    private @StringRes int mSecurityIconDescriptionRes;
    private @DrawableRes int mNavigationIconTintRes;

    private Resources mResources;
    private ToolbarCommonPropertiesModel mToolbarCommonPropertiesModel;

    StatusMediator(PropertyModel model, Resources resources) {
        mModel = model;
        updateColorTheme();

        mResources = resources;
    }

    /**
     * Set the ToolbarDataProvider for this class.
     */
    void setToolbarDataProvider(ToolbarCommonPropertiesModel toolbarCommonPropertiesModel) {
        mToolbarCommonPropertiesModel = toolbarCommonPropertiesModel;
    }

    /**
     * Toggle animations of icon changes.
     */
    void setAnimationsEnabled(boolean enabled) {
        mModel.set(StatusProperties.ANIMATIONS_ENABLED, enabled);
    }

    /**
     * Specify whether displayed page is an offline page.
     */
    void setPageIsOffline(boolean pageIsOffline) {
        if (mPageIsOffline != pageIsOffline) {
            mPageIsOffline = pageIsOffline;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify whether displayed page is a preview page.
     */
    void setPageIsPreview(boolean pageIsPreview) {
        if (mPageIsPreview != pageIsPreview) {
            mPageIsPreview = pageIsPreview;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify displayed page's security level.
     */
    void setPageSecurityLevel(@ConnectionSecurityLevel int level) {
        if (mPageSecurityLevel == level) return;
        mPageSecurityLevel = level;
        updateStatusVisibility();
        updateLocationBarIcon();
    }

    /**
     * Specify icon displayed by the security chip.
     */
    void setSecurityIconResource(@DrawableRes int securityIcon) {
        mSecurityIconRes = securityIcon;
        updateLocationBarIcon();
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconTint(@ColorRes int tintList) {
        mSecurityIconTintRes = tintList;
        updateLocationBarIcon();
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconDescription(@StringRes int desc) {
        mSecurityIconDescriptionRes = desc;
        updateLocationBarIcon();
    }

    /**
     * Specify minimum width of the separator field.
     */
    void setSeparatorFieldMinWidth(int width) {
        mSeparatorMinWidth = width;
    }

    /**
     * Specify whether status icon should be shown when URL is focused.
     */
    void setShowIconsWhenUrlFocused(boolean showIconWhenFocused) {
        mShowStatusIconWhenUrlFocused = showIconWhenFocused;
        updateLocationBarIcon();
    }

    /**
     * Specify object to receive status click events.
     *
     * @param listener Specifies target object to receive events.
     */
    void setStatusClickListener(View.OnClickListener listener) {
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, listener);
    }

    /**
     * Update unfocused location bar width to determine shape and content of the
     * Status view.
     */
    void setUnfocusedLocationBarWidth(int width) {
        // This unfocused width is used rather than observing #onMeasure() to avoid showing the
        // verbose status when the animation to unfocus the URL bar has finished. There is a call to
        // LocationBarLayout#onMeasure() after the URL focus animation has finished and before the
        // location bar has received its updated width layout param.
        int computedSpace = width - mUrlMinWidth - mSeparatorMinWidth;
        boolean hasSpaceForStatus = width >= mVerboseStatusTextMinWidth;

        if (hasSpaceForStatus) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH, computedSpace);
        }

        if (hasSpaceForStatus != mVerboseStatusSpaceAvailable) {
            mVerboseStatusSpaceAvailable = hasSpaceForStatus;
            updateStatusVisibility();
        }
    }

    /**
     * Report URL focus change.
     */
    void setUrlHasFocus(boolean urlHasFocus) {
        if (mUrlHasFocus == urlHasFocus) return;

        mUrlHasFocus = urlHasFocus;
        updateStatusVisibility();
        updateLocationBarIcon();
    }

    /**
     * Reports whether the first omnibox suggestion is a search query.
     */
    void setFirstSuggestionIsSearchType(boolean firstSuggestionIsSearchQuery) {
        mFirstSuggestionIsSearchQuery = firstSuggestionIsSearchQuery;
        updateLocationBarIcon();
    }

    /**
     * Specify minimum width of an URL field.
     */
    void setUrlMinWidth(int width) {
        mUrlMinWidth = width;
    }

    /**
     * Toggle between dark and light UI color theme.
     */
    void setUseDarkColors(boolean useDarkColors) {
        if (mDarkTheme != useDarkColors) {
            mDarkTheme = useDarkColors;
            updateColorTheme();
        }
    }

    /**
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, incognitoBadgeVisible);
    }

    /**
     * Specify minimum width of the verbose status text field.
     */
    void setVerboseStatusTextMinWidth(int width) {
        mVerboseStatusTextMinWidth = width;
    }

    /**
     * Update visibility of the verbose status text field.
     */
    private void updateStatusVisibility() {
        int statusText = 0;

        if (mPageIsPreview) {
            statusText = R.string.location_bar_preview_lite_page_status;
        } else if (mPageIsOffline) {
            statusText = R.string.location_bar_verbose_status_offline;
        }

        // Decide whether presenting verbose status text makes sense.
        boolean newVisibility = shouldShowVerboseStatusText() && mVerboseStatusSpaceAvailable
                && (!mUrlHasFocus) && (statusText != 0);

        // Update status content only if it is visible.
        // Note: PropertyModel will help us avoid duplicate updates with the
        // same value.
        if (newVisibility) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES, statusText);
        }

        mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE, newVisibility);
    }

    /**
     * Update color theme for all status components.
     */
    private void updateColorTheme() {
        @ColorRes
        int separatorColor =
                mDarkTheme ? R.color.divider_bg_color_dark : R.color.divider_bg_color_light;

        @ColorRes
        int textColor = 0;
        if (mPageIsPreview) {
            textColor = mDarkTheme ? R.color.locationbar_status_preview_color
                                   : R.color.locationbar_status_preview_color_light;
        } else if (mPageIsOffline) {
            textColor = mDarkTheme ? R.color.locationbar_status_offline_color
                                   : R.color.locationbar_status_offline_color_light;
        }

        @ColorRes
        int tintColor = ColorUtils.getThemedToolbarIconTintRes(!mDarkTheme);

        mModel.set(StatusProperties.SEPARATOR_COLOR_RES, separatorColor);
        mNavigationIconTintRes = tintColor;
        if (textColor != 0) mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES, textColor);

        updateLocationBarIcon();
    }

    /**
     * Reports whether security icon is shown.
     */
    @VisibleForTesting
    boolean isSecurityButtonShown() {
        return mIsSecurityButtonShown;
    }

    /**
     * Compute verbose status text for the current page.
     */
    private boolean shouldShowVerboseStatusText() {
        return (mPageIsPreview && mPageSecurityLevel != ConnectionSecurityLevel.DANGEROUS)
                || mPageIsOffline;
    }

    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mIsSearchEngineGoogle = isSearchEngineGoogle;
        updateLocationBarIcon();
    }

    /**
     * Update selection of icon presented on the location bar.
     *
     * - Navigation button is:
     *     - shown only on large form factor devices (tablets and up),
     *     - shown only if URL is focused.
     *
     * - Security icon is:
     *     - shown only if specified,
     *     - not shown if URL is focused.
     */
    private void updateLocationBarIcon() {
        // Update the accessibility description before continuing since we need it either way.
        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, getAccessibilityDescriptionRes());

        // When the search engine logo should be shown, but the engine isn't Google. In this case,
        // we download the icon on the fly.
        boolean showFocused = mUrlHasFocus && mShowStatusIconWhenUrlFocused;
        // Show the logo unfocused if "Query in the omnibox" is active or we're on the NTP. Current
        // "Query in the omnibox" behavior makes it active for non-dse searches if you've just
        // changed your default search engine.The included workaround below
        // (doesUrlMatchDefaultSearchEngine) can be removed once this is fixed.
        // TODO(crbug.com/991017): Remove doesUrlMatchDefaultSearchEngine when "Query in the
        //                         omnibox" properly reacts to dse changes.
        boolean showUnfocusedNewTabPage = !mUrlHasFocus && mToolbarCommonPropertiesModel != null
                && mToolbarCommonPropertiesModel.getNewTabPageForCurrentTab() != null;
        boolean showUnfocusedSearchResultsPage = !mUrlHasFocus
                && mToolbarCommonPropertiesModel != null
                && mToolbarCommonPropertiesModel.getDisplaySearchTerms() != null
                && SearchEngineLogoUtils.doesUrlMatchDefaultSearchEngine(
                        mToolbarCommonPropertiesModel.getCurrentUrl());
        if (SearchEngineLogoUtils.shouldShowSearchEngineLogo()
                && (showFocused || showUnfocusedNewTabPage || showUnfocusedSearchResultsPage)) {
            mShouldCancelCustomFavicon = false;
            mModel.set(StatusProperties.STATUS_ICON_TINT_RES, 0);
            if (mIsSearchEngineGoogle) {
                mModel.set(StatusProperties.STATUS_ICON_RES,
                        SearchEngineLogoUtils.shouldShowSearchLoupeEverywhere()
                                ? R.drawable.omnibox_search
                                : R.drawable.ic_logo_googleg_24dp);
            } else {
                mModel.set(StatusProperties.STATUS_ICON_RES, R.drawable.ic_search);
                if (!SearchEngineLogoUtils.shouldShowSearchLoupeEverywhere()) {
                    SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                            Profile.getLastUsedProfile().getOriginalProfile(), mResources,
                            (favicon) -> {
                                if (favicon == null || mShouldCancelCustomFavicon) return;
                                mModel.set(StatusProperties.STATUS_ICON, favicon);
                            });
                }
            }
            return;
        } else {
            mShouldCancelCustomFavicon = true;
        }

        int icon = 0;
        int tint = 0;
        int toast = 0;

        mIsSecurityButtonShown = false;
        if (mUrlHasFocus) {
            if (mShowStatusIconWhenUrlFocused) {
                icon = mFirstSuggestionIsSearchQuery ? R.drawable.omnibox_search
                                                     : R.drawable.ic_omnibox_page;
                tint = mNavigationIconTintRes;
            }
        } else if (mSecurityIconRes != 0) {
            mIsSecurityButtonShown = true;
            icon = mSecurityIconRes;
            tint = mSecurityIconTintRes;
            toast = R.string.menu_page_info;
        }

        if (mPageIsPreview) {
            tint = mDarkTheme ? R.color.locationbar_status_preview_color
                              : R.color.locationbar_status_preview_color_light;
        }

        mModel.set(StatusProperties.STATUS_ICON_RES, icon);
        mModel.set(StatusProperties.STATUS_ICON_TINT_RES, tint);
        mModel.set(StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES, toast);
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
        if (mUrlHasFocus) {
            if (SearchEngineLogoUtils.shouldShowSearchEngineLogo()) {
                return 0;
            } else if (mShowStatusIconWhenUrlFocused) {
                return R.string.accessibility_toolbar_btn_site_info;
            }
        } else if (mSecurityIconRes != 0) {
            return mSecurityIconDescriptionRes;
        }

        return 0;
    }
}
