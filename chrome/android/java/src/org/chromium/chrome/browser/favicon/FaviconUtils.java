// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.favicon;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.util.ViewUtils;

/**
 * Utilities to deal with favicons.
 */
public class FaviconUtils {
    /**
     * Creates a {@link RoundedIconGenerator} to generate circular {@link Bitmap}s of favicons.
     * @param resources The {@link Resources} for accessing color and dimen resources.
     * @return A {@link RoundedIconGenerator} that uses the default circle icon style. Intended for
     *         monograms, e.g. a circle with character(s) in the center.
     */
    public static RoundedIconGenerator createCircularIconGenerator(Resources resources) {
        int displayedIconSize = resources.getDimensionPixelSize(R.dimen.circular_monogram_size);
        int cornerRadius = displayedIconSize / 2;
        int textSize = resources.getDimensionPixelSize(R.dimen.circular_monogram_text_size);
        return new RoundedIconGenerator(displayedIconSize, displayedIconSize, cornerRadius,
                getIconColor(resources), textSize);
    }

    /**
     * Creates a {@link RoundedIconGenerator} to generate rounded rectangular {@link Bitmap}s of
     * favicons.
     * @param resources The {@link Resources} for accessing color and dimen resources.
     * @return A {@link RoundedIconGenerator} that uses the default rounded rectangle icon style.
     *         Intended for monograms, e.g. a rounded rectangle with character(s) in the center.
     */
    public static RoundedIconGenerator createRoundedRectangleIconGenerator(Resources resources) {
        int displayedIconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        int cornerRadius = resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        int textSize = resources.getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        return new RoundedIconGenerator(displayedIconSize, displayedIconSize, cornerRadius,
                getIconColor(resources), textSize);
    }

    /**
     * Creates a {@link RoundedBitmapDrawable} using the provided {@link Bitmap} and a default
     * favicon corner radius.
     * @param resources The {@link Resources}.
     * @param icon The {@link Bitmap} to round.
     * @return A {@link RoundedBitmapDrawable} for the provided {@link Bitmap}.
     */
    public static RoundedBitmapDrawable createRoundedBitmapDrawable(
            Resources resources, Bitmap icon) {
        return ViewUtils.createRoundedBitmapDrawable(resources, icon,
                resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius));
    }

    private static int getIconColor(Resources resources) {
        return ApiCompatibilityUtils.getColor(resources, R.color.default_favicon_background_color);
    }
}
