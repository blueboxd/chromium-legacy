// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.ColorStateList;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.ConnectionSecurityLevel;

/**
 * Defines the data that is exposed to properly render the Toolbar.
 */
public interface ToolbarDataProvider {
    /**
     * @return The tab that contains the information currently displayed in the toolbar.
     */
    @Nullable
    Tab getTab();

    /**
     * @return Whether ToolbarDataProvider currently has a tab related to it.
     */
    boolean hasTab();

    /**
     * @return The current url for the current tab. Returns empty string when there is no tab.
     */
    @NonNull
    String getCurrentUrl();

    /**
     * @return The NewTabPage shown for the current Tab or null if one is not being shown.
     */
    NewTabPage getNewTabPageForCurrentTab();

    /**
     * @return Whether the toolbar is currently being displayed for incognito.
     */
    boolean isIncognito();

    /**
     * @return Whether the toolbar is currently being displayed in overview mode and showing the
     *  omnibox.
     */
    boolean isInOverviewAndShowingOmnibox();

    /**
     * @return Whether the location bar should show when in overview mode.
     */
    boolean shouldShowLocationBarInOverviewMode();

    /**
     * @return The current {@link Profile}.
     */
    Profile getProfile();

    /**
     * @return The contents of the {@link org.chromium.chrome.browser.omnibox.UrlBar}.
     */
    UrlBarData getUrlBarData();

    /**
     * @return The title of the current tab, or the empty string if there is currently no tab.
     */
    String getTitle();

    /**
     * @return The primary color to use for the background drawable.
     */
    int getPrimaryColor();

    /**
     * @return Whether the current primary color is a brand color.
     */
    boolean isUsingBrandColor();

    /**
     * @return Whether the page currently shown is an offline page.
     */
    boolean isOfflinePage();

    /**
     * @return Whether the page currently shown is a preview.
     */
    boolean isPreview();

    /**
     * @return The current {@link ConnectionSecurityLevel}.
     */
    @ConnectionSecurityLevel
    int getSecurityLevel();

    /**
     * @param isFocusedFromFakebox If the omnibox focus originated from the fakebox.
     * @return The current page classification.
     */
    default int getPageClassification(boolean isFocusedFromFakebox) {
        return 0;
    }

    /**
     * @return The resource ID of the icon that should be displayed or 0 if no icon should be shown.
     */
    @DrawableRes
    int getSecurityIconResource(boolean isTablet);

    /**
     * @return The resource ID of the content description for the security icon.
     */
    @StringRes
    default int getSecurityIconContentDescription() {
        switch (getSecurityLevel()) {
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.HTTP_SHOW_WARNING:
                return R.string.accessibility_security_btn_warn;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.string.accessibility_security_btn_dangerous;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
            case ConnectionSecurityLevel.EV_SECURE:
                return R.string.accessibility_security_btn_secure;
            default:
                assert false;
        }
        return 0;
    }

    /**
     * @return The {@link ColorStateList} to use to tint the security state icon.
     */
    @ColorRes
    int getSecurityIconColorStateList();

    /**
     * If the current tab state is eligible for displaying the search query terms instead of the
     * URL, this extracts the query terms from the current URL.
     *
     * @return The search terms. Returns null if the tab is ineligible to display the search terms
     *         instead of the URL.
     */
    @Nullable
    default public String getDisplaySearchTerms() {
        return null;
    }

    /**
     * Update the information required to display the search engine logo in the omnibox.
     *
     * @param shouldShowSearchEngineLogo True if we should show the search engine logo in the
     *         omnibox.
     * @param isSearchEngineGoogle True if the default search engine is Google.
     * @param searchEngineUrl The url for the search engine, used to fetch the favicon.
     */
    void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl);
}
