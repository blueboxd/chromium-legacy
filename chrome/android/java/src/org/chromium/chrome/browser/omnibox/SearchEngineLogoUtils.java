// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.util.UrlUtilities;

/**
 * Collection of shared code for displaying search engine logos.
 */
public class SearchEngineLogoUtils {
    private static FaviconHelper sFaviconHelper;

    // Cache these values so they don't need to be recalculated.
    private static int sSearchEngineLogoTargetSizePixels;

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @return True if we should show the search engine logo.
     */
    public static boolean shouldShowSearchEngineLogo() {
        return !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                && ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO);
    }

    /**
     * @return True if the search engine is Google.
     */
    public static boolean isSearchEngineGoogle() {
        return TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
    }

    /**
     * @return The search URL of the current DSE.
     */
    public static String getSearchLogoUrl() {
        return UrlUtilities.stripPath(
                TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl().getURL());
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The size that the logo should be displayed as.
     */
    public static int getSearchEngineLogoSizePixels(Resources resources) {
        if (sSearchEngineLogoTargetSizePixels == 0) {
            sSearchEngineLogoTargetSizePixels = resources.getDimensionPixelSize(
                    R.dimen.omnibox_search_engine_logo_favicon_size);
        }

        return sSearchEngineLogoTargetSizePixels;
    }

    public static void getSearchEngineLogoFavicon(
            Profile profile, Resources resources, Callback<Bitmap> callback) {
        if (sFaviconHelper == null) sFaviconHelper = new FaviconHelper();

        boolean willReturn = sFaviconHelper.getLocalFaviconImageForURL(profile, getSearchLogoUrl(),
                getSearchEngineLogoSizePixels(resources), (image, iconUrl) -> {
                    if (image == null) {
                        callback.onResult(image);
                        return;
                    }

                    callback.onResult(Bitmap.createScaledBitmap(image,
                            SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources),
                            SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources), true));
                });
        if (!willReturn) callback.onResult(null);
    }
}
