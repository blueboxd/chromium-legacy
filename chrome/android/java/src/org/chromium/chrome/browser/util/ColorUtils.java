// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;

/**
 * Helper functions for working with colors.
 */
public class ColorUtils {
    private static final float CONTRAST_LIGHT_ITEM_THRESHOLD = 3f;
    private static final float LIGHTNESS_OPAQUE_BOX_THRESHOLD = 0.82f;
    private static final float LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA = 0.2f;
    private static final float MAX_LUMINANCE_FOR_VALID_THEME_COLOR = 0.94f;
    private static final float THEMED_FOREGROUND_BLACK_FRACTION = 0.64f;

    /** Percentage to darken a color by when setting the status bar color. */
    private static final float DARKEN_COLOR_FRACTION = 0.6f;

    /**
     * Computes the lightness value in HSL standard for the given color.
     */
    public static float getLightnessForColor(int color) {
        int red = Color.red(color);
        int green = Color.green(color);
        int blue = Color.blue(color);
        int largest = Math.max(red, Math.max(green, blue));
        int smallest = Math.min(red, Math.min(green, blue));
        int average = (largest + smallest) / 2;
        return average / 255.0f;
    }

    /** Calculates the contrast between the given color and white, using the algorithm provided by
     * the WCAG v2 in http://www.w3.org/TR/WCAG20/#contrast-ratiodef.
     */
    private static float getContrastForColor(int color) {
        float bgR = Color.red(color) / 255f;
        float bgG = Color.green(color) / 255f;
        float bgB = Color.blue(color) / 255f;
        bgR = (bgR < 0.03928f) ? bgR / 12.92f : (float) Math.pow((bgR + 0.055f) / 1.055f, 2.4f);
        bgG = (bgG < 0.03928f) ? bgG / 12.92f : (float) Math.pow((bgG + 0.055f) / 1.055f, 2.4f);
        bgB = (bgB < 0.03928f) ? bgB / 12.92f : (float) Math.pow((bgB + 0.055f) / 1.055f, 2.4f);
        float bgL = 0.2126f * bgR + 0.7152f * bgG + 0.0722f * bgB;
        return Math.abs((1.05f) / (bgL + 0.05f));
    }

    /**
     * Determines the default theme color used for toolbar based on the provided parameters.
     * @param res {@link Resources} used to retrieve colors.
     * @param isIncognito Whether to retrieve the default theme color for incognito mode.
     * @return The default theme color.
     */
    public static int getDefaultThemeColor(Resources res, boolean isIncognito) {
        return isIncognito
                ? ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary_incognito)
                : ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary);
    }

    /**
     * Returns the primary background color used as native page background based on the given
     * parameters.
     * @param res The {@link Resources} used to retrieve colors.
     * @param isIncognito Whether or not the color is for incognito mode.
     * @return The primary background color.
     */
    public static int getPrimaryBackgroundColor(Resources res, boolean isIncognito) {
        return isIncognito
                ? ApiCompatibilityUtils.getColor(res, R.color.incognito_modern_primary_color)
                : ApiCompatibilityUtils.getColor(res, R.color.modern_primary_color);
    }

    /**
     * Returns the icon tint resource to use based on the current parameters and whether the app is
     * in night mode.
     * @param useLight Whether or not the icon tint should be light when not in night mode.
     * @return The {@link ColorRes} for the icon tint.
     */
    public static @ColorRes int getIconTintRes(boolean useLight) {
        return useLight ? R.color.tint_on_dark_bg : R.color.standard_mode_tint;
    }

    /**
     * Returns the icon tint to use based on the current parameters and whether the app is in night
     * mode.
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light when not in night mode.
     * @return The {@link ColorStateList} for the icon tint.
     */
    public static ColorStateList getIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getIconTintRes(useLight));
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorRes} for the icon tint of themed toolbar.
     */
    public static @ColorRes int getThemedToolbarIconTintRes(boolean useLight) {
        // Light toolbar theme colors may be used in night mode, so use toolbar_icon_tint_dark which
        // is not overridden in night- resources.
        return useLight ? R.color.tint_on_dark_bg : R.color.toolbar_icon_tint_dark;
    }

    /**
     * Returns the icon tint for based on the given parameters. Does not adjust color based on
     * night mode as this may conflict with toolbar theme colors.
     * @param context The {@link Context} used to retrieve colors.
     * @param useLight Whether or not the icon tint should be light.
     * @return The {@link ColorStateList} for the icon tint of themed toolbar.
     */
    public static ColorStateList getThemedToolbarIconTint(Context context, boolean useLight) {
        return AppCompatResources.getColorStateList(context, getThemedToolbarIconTintRes(useLight));
    }

    /**
     * Determine the text box color based on the current toolbar background color.
     * @param res {@link Resources} used to retrieve colors.
     * @param isLocationBarShownInNtp Whether the location bar is currently shown in an NTP. Note
     *                                that this should be false if the returned text box color is
     *                                not used in an NTP.
     * @param color The color of the toolbar background.
     * @param isIncognito Whether or not the color is used for incognito mode.
     * @return The base color for the textbox given a toolbar background color.
     */
    public static int getTextBoxColorForToolbarBackground(
            Resources res, boolean isLocationBarShownInNtp, int color, boolean isIncognito) {
        // Text box color on default toolbar background in incognito mode is a pre-defined
        // color. We calculate the equivalent opaque color from the pre-defined translucent color.
        if (isIncognito) {
            final int overlayColor = ApiCompatibilityUtils.getColor(
                    res, R.color.toolbar_text_box_background_incognito);
            final float overlayColorAlpha = Color.alpha(overlayColor) / 255f;
            final int overlayColorOpaque = overlayColor & 0xFF000000;
            return getColorWithOverlay(color, overlayColorOpaque, overlayColorAlpha);
        }

        // NTP should have no visible text box in the toolbar, so just return the NTP
        // background color.
        if (isLocationBarShownInNtp) return getPrimaryBackgroundColor(res, false);

        // Text box color on default toolbar background in standard mode is a pre-defined
        // color instead of a calculated color.
        if (ColorUtils.isUsingDefaultToolbarColor(res, false, color)) {
            return ApiCompatibilityUtils.getColor(res, R.color.toolbar_text_box_background);
        }

        // TODO(mdjones): Clean up shouldUseOpaqueTextboxBackground logic.
        if (shouldUseOpaqueTextboxBackground(color)) return Color.WHITE;

        return getColorWithOverlay(color, Color.WHITE, LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA);
    }

    /**
     * @param tab The {@link Tab} on which the toolbar scene layer color is used.
     * @return The toolbar (or browser controls) color used in the compositor scene layer. Note that
     *         this is primarily used for compositor animation, and doesn't affect the Android view.
     */
    public static @ColorInt int getToolbarSceneLayerBackground(Tab tab) {
        // On NTP, the toolbar background is tinted as the NTP background color, so return NTP
        // background color here to make animation smoother.
        if (tab.getNativePage() instanceof NewTabPage) {
            if (((NewTabPage) tab.getNativePage()).isLocationBarShownInNTP()) {
                return tab.getNativePage().getBackgroundColor();
            }
        }

        return TabThemeColorHelper.getColor(tab);
    }

    /**
     * @return Alpha for the textbox given a Tab.
     */
    public static float getTextBoxAlphaForToolbarBackground(Tab tab) {
        if (tab.getNativePage() instanceof NewTabPage) {
            if (((NewTabPage) tab.getNativePage()).isLocationBarShownInNTP()) return 0f;
        }
        int color = TabThemeColorHelper.getColor(tab);
        return shouldUseOpaqueTextboxBackground(color)
                ? 1f : LOCATION_BAR_TRANSPARENT_BACKGROUND_ALPHA;
    }

    /**
     * Get a color when overlayed with a different color.
     * @param baseColor The base Android color.
     * @param overlayColor The overlay Android color.
     * @param overlayAlpha The alpha |overlayColor| should have on the base color.
     */
    public static int getColorWithOverlay(int baseColor, int overlayColor, float overlayAlpha) {
        return Color.rgb(
                (int) MathUtils.interpolate(Color.red(baseColor), Color.red(overlayColor),
                        overlayAlpha),
                (int) MathUtils.interpolate(Color.green(baseColor), Color.green(overlayColor),
                        overlayAlpha),
                (int) MathUtils.interpolate(Color.blue(baseColor), Color.blue(overlayColor),
                        overlayAlpha));
    }

    /**
     * Darkens the given color to use on the status bar.
     * @param color Color which should be darkened.
     * @return Color that should be used for Android status bar.
     */
    public static int getDarkenedColorForStatusBar(int color) {
        return getDarkenedColor(color, DARKEN_COLOR_FRACTION);
    }

    /**
     * Darken a color to a fraction of its current brightness.
     * @param color The input color.
     * @param darkenFraction The fraction of the current brightness the color should be.
     * @return The new darkened color.
     */
    public static int getDarkenedColor(int color, float darkenFraction) {
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        hsv[2] *= darkenFraction;
        return Color.HSVToColor(hsv);
    }

    /**
     * Check whether lighter or darker foreground elements (i.e. text, drawables etc.)
     * should be used depending on the given background color.
     * @param backgroundColor The background color value which is being queried.
     * @return Whether light colored elements should be used.
     */
    public static boolean shouldUseLightForegroundOnBackground(int backgroundColor) {
        return getContrastForColor(backgroundColor) >= CONTRAST_LIGHT_ITEM_THRESHOLD;
    }

    /**
     * Check which version of the textbox background should be used depending on the given
     * color.
     * @param color The color value we are querying for.
     * @return Whether the transparent version of the background should be used.
     */
    public static boolean shouldUseOpaqueTextboxBackground(int color) {
        return getLightnessForColor(color) > LIGHTNESS_OPAQUE_BOX_THRESHOLD;
    }

    /**
     * Returns an opaque version of the given color.
     * @param color Color for which an opaque version should be returned.
     * @return Opaque version of the given color.
     */
    public static int getOpaqueColor(int color) {
        return Color.rgb(Color.red(color), Color.green(color), Color.blue(color));
    }

    /**
     * Test if the toolbar is using the default color.
     * @param resources The resources to get the toolbar primary color.
     * @param isIncognito Whether to retrieve the default theme color for incognito mode.
     * @param color The color that the toolbar is using.
     * @return If the color is the default toolbar color.
     */
    public static boolean isUsingDefaultToolbarColor(
            Resources resources, boolean isIncognito, int color) {
        return color == getDefaultThemeColor(resources, isIncognito);
    }

    /**
     * Determine if a theme color is valid. A theme color is invalid if its luminance is > 0.94.
     * @param color The color to test.
     * @return True if the theme color is valid.
     */
    public static boolean isValidThemeColor(int color) {
        return ColorUtils.getLightnessForColor(color) <= MAX_LUMINANCE_FOR_VALID_THEME_COLOR;
    }

    /**
     * Compute a color to use for assets that sit on top of a themed background.
     * @param themeColor The base theme color.
     * @return A color to use for elements in the foreground (on top of the base theme color).
     */
    public static int getThemedAssetColor(int themeColor, boolean isIncognito) {
        if (ColorUtils.shouldUseLightForegroundOnBackground(themeColor) || isIncognito) {
            // Dark theme.
            return Color.WHITE;
        } else {
            // Light theme.
            return ColorUtils.getColorWithOverlay(themeColor, Color.BLACK,
                    THEMED_FOREGROUND_BLACK_FRACTION);
        }
    }
}
