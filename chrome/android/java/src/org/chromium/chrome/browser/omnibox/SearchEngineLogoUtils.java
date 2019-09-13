// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.HashMap;
import java.util.Map;

/**
 * Collection of shared code for displaying search engine logos.
 */
public class SearchEngineLogoUtils {
    private static final String ROUNDED_EDGES_VARIANT = "rounded_edges";
    private static final String LOUPE_EVERYWHERE_VARIANT = "loupe_everywhere";
    private static final String DUMMY_URL_QUERY = "replace_me";

    // Cache the logo and return it when the logo url that's cached matches the current logo url.
    private static Bitmap sCachedComposedBackground;
    private static String sCachedComposedBackgroundLogoUrl;
    private static FaviconHelper sFaviconHelper;
    private static RoundedIconGenerator sRoundedIconGenerator;

    // Cache these values so they don't need to be recalculated.
    private static int sSearchEngineLogoTargetSizePixels;
    private static int sSearchEngineLogoComposedSizePixels;

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @return True if we should show the search engine logo.
     */
    public static boolean shouldShowSearchEngineLogo() {
        return !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                && ChromeFeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO)
                // Using the profile now, so we need to pay attention to browser initialization.
                && BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                           .isFullBrowserStarted()
                && !Profile.getLastUsedProfile().isOffTheRecord();
    }

    public static boolean shouldRoundedSearchEngineLogo() {
        return shouldShowSearchEngineLogo() && ChromeFeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO, ROUNDED_EDGES_VARIANT, false);
    }

    public static boolean shouldShowSearchLoupeEverywhere() {
        return shouldShowSearchEngineLogo()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO, LOUPE_EVERYWHERE_VARIANT,
                        false);
    }

    /**
     * @return True if the search engine is Google.
     */
    public static boolean isSearchEngineGoogle() {
        return TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
    }

    /**
     * @return True if the given url is the same domain as the DSE.
     */
    public static boolean doesUrlMatchDefaultSearchEngine(String url) {
        if (TextUtils.isEmpty(url)) return false;
        return UrlUtilities.sameDomainOrHost(url, getSearchLogoUrl(), false);
    }

    /**
     * @return The search URL of the current DSE or null if one cannot be found.
     */
    @Nullable
    public static String getSearchLogoUrl() {
        String logoUrlWithPath =
                TemplateUrlServiceFactory.get().getUrlForSearchQuery(DUMMY_URL_QUERY);
        if (logoUrlWithPath == null || !UrlUtilities.isHttpOrHttps(logoUrlWithPath)) {
            return logoUrlWithPath;
        }

        return UrlUtilities.stripPath(logoUrlWithPath);
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The size that the logo favicon should be.
     */
    public static int getSearchEngineLogoSizePixels(Resources resources) {
        if (sSearchEngineLogoTargetSizePixels == 0) {
            if (shouldRoundedSearchEngineLogo()) {
                sSearchEngineLogoTargetSizePixels = resources.getDimensionPixelSize(
                        R.dimen.omnibox_search_engine_logo_favicon_size);
            } else {
                sSearchEngineLogoTargetSizePixels =
                        getSearchEngineLogoComposedSizePixels(resources);
            }
        }

        return sSearchEngineLogoTargetSizePixels;
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The total size the logo will be on screen.
     */
    public static int getSearchEngineLogoComposedSizePixels(Resources resources) {
        if (sSearchEngineLogoComposedSizePixels == 0) {
            sSearchEngineLogoComposedSizePixels = resources.getDimensionPixelSize(
                    R.dimen.omnibox_search_engine_logo_composed_size);
        }

        return sSearchEngineLogoComposedSizePixels;
    }

    /**
     * Get the search engine logo favicon. This can return a null bitmap under certain
     * circumstances, such as: no logo url found, network/cache error, etc.
     *
     * @param profile The current profile.
     * @param resources Provides access to Android resources.
     * @param callback How the bitmap will be returned to the caller.
     */
    public static void getSearchEngineLogoFavicon(
            Profile profile, Resources resources, Callback<Bitmap> callback) {
        if (sFaviconHelper == null) sFaviconHelper = new FaviconHelper();

        String logoUrl = getSearchLogoUrl();
        if (logoUrl == null) {
            callback.onResult(null);
            return;
        }

        // Return a cached copy if it's available.
        if (sCachedComposedBackground != null
                && sCachedComposedBackgroundLogoUrl.equals(getSearchLogoUrl())) {
            callback.onResult(sCachedComposedBackground);
            return;
        }

        final int logoSizePixels = SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources);
        boolean willReturn = sFaviconHelper.getLocalFaviconImageForURL(
                profile, logoUrl, logoSizePixels, (image, iconUrl) -> {
                    if (image == null) {
                        callback.onResult(image);
                        return;
                    }

                    processReturnedLogo(logoUrl, image, resources, callback);
                });
        if (!willReturn) callback.onResult(null);
    }

    /**
     * Process the image returned from a network fetch or cache hit. This method processes the logo
     * to make it eligible for display. The logo is resized to ensure it will fill the required
     * size. This is done because the icon returned from native could be a different size. If the
     * rounded edges variant is active, then a smaller icon is downloaded and drawn on top of a
     * circle background. This looks better and also has more predictable behavior than rounding the
     * edges of the full size icon. The circle background is a solid color made up of the result
     * from a call to getMostCommonEdgeColor(...).
     * @param logoUrl The url for the given logo.
     * @param image The logo to process.
     * @param resources Android resources object used to access dimensions.
     * @param callback The client callback to receive the processed logo.
     */
    private static void processReturnedLogo(
            String logoUrl, Bitmap image, Resources resources, Callback<Bitmap> callback) {
        // Scale the logo up to the desired size.
        int logoSizePixels = SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources);
        Bitmap scaledIcon = Bitmap.createScaledBitmap(image,
                SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources),
                SearchEngineLogoUtils.getSearchEngineLogoSizePixels(resources), true);

        Bitmap composedIcon = scaledIcon;
        if (shouldRoundedSearchEngineLogo()) {
            int composedSizePixels = getSearchEngineLogoComposedSizePixels(resources);
            if (sRoundedIconGenerator == null) {
                sRoundedIconGenerator = new RoundedIconGenerator(composedSizePixels,
                        composedSizePixels, composedSizePixels, Color.TRANSPARENT, 0);
            }
            sRoundedIconGenerator.setBackgroundColor(getMostCommonEdgeColor(image));

            // Generate a rounded background with no text.
            composedIcon = sRoundedIconGenerator.generateIconForText("");
            Canvas canvas = new Canvas(composedIcon);
            // Draw the logo in the middle of the generated background.
            int dx = (composedSizePixels - logoSizePixels) / 2;
            canvas.drawBitmap(scaledIcon, dx, dx, null);
        }
        // Cache the result icon to reduce future work.
        sCachedComposedBackground = composedIcon;
        sCachedComposedBackgroundLogoUrl = logoUrl;

        callback.onResult(sCachedComposedBackground);
    }

    /**
     * Samples the edges of given bitmap and returns the most common color.
     * @param icon Bitmap to be sampled.
     */
    @VisibleForTesting
    static int getMostCommonEdgeColor(Bitmap icon) {
        Map<Integer, Integer> colorCount = new HashMap<>();
        for (int i = 0; i < icon.getWidth(); i++) {
            // top edge
            int color = icon.getPixel(i, 0);
            System.out.println("current color: " + color);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // bottom edge
            color = icon.getPixel(i, icon.getHeight() - 1);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // Measure the lateral edges offset by 1 on each side.
            if (i > 0 && i < icon.getWidth() - 1) {
                // left edge
                color = icon.getPixel(0, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);

                // right edge
                color = icon.getPixel(icon.getWidth() - 1, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);
            }
        }

        // Find the most common color out of the map.
        int maxKey = Color.TRANSPARENT;
        int maxVal = -1;
        for (int color : colorCount.keySet()) {
            int count = colorCount.get(color);
            if (count > maxVal) {
                maxKey = color;
                maxVal = count;
            }
        }
        assert maxVal > -1;

        return maxKey;
    }
}
