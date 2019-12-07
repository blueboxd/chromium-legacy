// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.support.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider.GoogleSansTypeface;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;

/** Provide Assets from the host */
public class AssetProvider {
    @VisibleForTesting
    final ImageLoader mImageLoader;
    @VisibleForTesting
    final StringFormatter mStringFormatter;
    private final Supplier<Integer> mDefaultCornerRadiusSupplier;
    private final Supplier<Boolean> mIsDarkThemeSupplier;
    private final Supplier<Long> mFadeImageThresholdMsSupplier;
    private final Supplier<Boolean> mIsRtLSupplier;
    @VisibleForTesting
    final TypefaceProvider mTypefaceProvider;

    public AssetProvider(ImageLoader imageLoader, StringFormatter stringFormatter,
            Supplier<Integer> defaultCornerRadiusSupplier,
            Supplier<Long> fadeImageThresholdMsSupplier, Supplier<Boolean> isDarkThemeSupplier,
            Supplier<Boolean> isRtLSupplier, TypefaceProvider typefaceProvider) {
        this.mImageLoader = imageLoader;
        this.mStringFormatter = stringFormatter;
        this.mDefaultCornerRadiusSupplier = defaultCornerRadiusSupplier;
        this.mIsDarkThemeSupplier = isDarkThemeSupplier;
        this.mFadeImageThresholdMsSupplier = fadeImageThresholdMsSupplier;
        this.mIsRtLSupplier = isRtLSupplier;
        this.mTypefaceProvider = typefaceProvider;
    }

    /**
     * Given an {@link Image}, asynchronously load the {@link Drawable} and return via a {@link
     * Consumer}.
     *
     * <p>The width and the height of the image can be provided preemptively, however it is not
     * guaranteed that both dimensions will be known. In the case that only one dimension is known,
     * the host should be careful to preserve the aspect ratio.
     *
     * @param image The image to load.
     * @param widthPx The width of the {@link Image} in pixels. Will be {@link #DIMENSION_NOT_SET}
     *         if
     *     unknown.
     * @param heightPx The height of the {@link Image} in pixels. Will be {@link #DIMENSION_NOT_SET}
     *     if unknown.
     * @param consumer Callback to return the {@link Drawable} from an {@link Image} if the load
     *     succeeds. {@literal null} should be passed to this if no source succeeds in loading the
     *     image
     */
    public void getImage(
            Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
        mImageLoader.getImage(image, widthPx, heightPx, consumer);
    }

    /** Return a relative elapsed time string such as "8 minutes ago" or "1 day ago". */
    public String getRelativeElapsedString(long elapsedTimeMillis) {
        return mStringFormatter.getRelativeElapsedString(elapsedTimeMillis);
    }

    /** Returns the default corner rounding radius in pixels. */
    public int getDefaultCornerRadius() {
        return mDefaultCornerRadiusSupplier.get();
    }

    /** Returns whether the theme for the Piet rendering context is a "dark theme". */
    public boolean isDarkTheme() {
        return mIsDarkThemeSupplier.get();
    }

    /**
     * Fade-in animation will only occur if image loading time takes more than this amount of time.
     */
    public long getFadeImageThresholdMs() {
        return mFadeImageThresholdMsSupplier.get();
    }

    /**
     * Allows the host to return a typeface Piet would otherwise not be able to access (ex. from
     * assets). Piet will call this when the typeface is not one Piet recognizes (as a default
     * Android typeface). If host does not specially handle the specified typeface, host can return
     * {@code null}, and Piet will proceed through its fallback typefaces.
     *
     * <p>Piet also expects the host to provide the Google Sans typeface, and will request it using
     * the {@link GoogleSansTypeface} StringDef. Piet will report errors if Google Sans is requested
     * and not found.
     */
    public void getTypeface(
            String typeface, boolean isItalic, Consumer</*@Nullable*/ Typeface> consumer) {
        mTypefaceProvider.getTypeface(typeface, isItalic, consumer);
    }

    /** Returns whether Piet should render layouts using a right-to-left orientation. */
    public boolean isRtL() {
        return mIsRtLSupplier.get();
    }

    /** Returns whether Piet should render layouts using a right-to-left orientation. */
    public Supplier<Boolean> isRtLSupplier() {
        return mIsRtLSupplier;
    }
}
