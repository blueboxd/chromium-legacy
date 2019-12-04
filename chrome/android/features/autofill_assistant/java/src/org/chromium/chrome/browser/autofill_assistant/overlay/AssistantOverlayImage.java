// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;

import androidx.annotation.ColorInt;

/** Java equivalent to {@code OverlayImageProto}. */
public class AssistantOverlayImage {
    /** The url of the image to display. */
    public final String mImageUrl;
    /** The image to display, after {@code mImageUrl} has been resolved. */
    public @Nullable Bitmap mImageBitmap;
    /** The size of the image to display. */
    public final int mImageSizeInPixels;
    /** The margin between the top of the page (anchor) and the image. */
    public final int mImageTopMarginInPixels;
    /** The margin between image and text. */
    public final int mImageBottomMarginInPixels;
    /** The text to display beneath the image. */
    public final String mText;
    /** The color of the text to draw */
    public final @Nullable @ColorInt Integer mTextColor;
    /** The size of the text to display. */
    public final int mTextSizeInPixels;

    public AssistantOverlayImage(String imageUrl, int imageSizeInPixels, int imageTopMarginInPixels,
            int imageBottomMarginInPixels, String text, @Nullable @ColorInt Integer textColor,
            int textSizeInPixels) {
        mImageUrl = imageUrl;
        mImageSizeInPixels = imageSizeInPixels;
        mImageTopMarginInPixels = imageTopMarginInPixels;
        mImageBottomMarginInPixels = imageBottomMarginInPixels;
        mText = text;
        mTextColor = textColor;
        mTextSizeInPixels = textSizeInPixels;
    }
}
