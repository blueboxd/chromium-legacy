// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.support.annotation.ColorInt;
import android.support.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;

/** Represents a view background. */
@JNINamespace("autofill_assistant")
public abstract class AssistantDrawable {
    /** Fetches the drawable. */
    public abstract void getDrawable(Context context, Callback<Drawable> callback);

    @CalledByNative
    public static AssistantDrawable createRectangleShape(
            @Nullable @ColorInt Integer backgroundColor, @Nullable @ColorInt Integer strokeColor,
            int strokeWidthInPixels, int cornerRadiusInPixels) {
        return new AssistantRectangleDrawable(
                backgroundColor, strokeColor, strokeWidthInPixels, cornerRadiusInPixels);
    }

    @CalledByNative
    public static AssistantDrawable createFromUrl(
            String url, int widthInPixels, int heightInPixels) {
        return new AssistantBitmapDrawable(url, widthInPixels, heightInPixels);
    }

    /** Returns whether {@code resourceId} is a valid resource identifier. */
    @CalledByNative
    public static boolean isValidDrawableResource(Context context, String resourceId) {
        int drawableId = context.getResources().getIdentifier(
                resourceId, "drawable", context.getPackageName());
        if (drawableId == 0) {
            return false;
        }
        try {
            ApiCompatibilityUtils.getDrawable(context.getResources(), drawableId);
            return true;
        } catch (Resources.NotFoundException e) {
            return false;
        }
    }

    @CalledByNative
    public static AssistantDrawable createFromResource(String resourceId) {
        return new AssistantResourceDrawable(resourceId);
    }

    private static class AssistantRectangleDrawable extends AssistantDrawable {
        private final @Nullable @ColorInt Integer mBackgroundColor;
        private final @Nullable @ColorInt Integer mStrokeColor;
        private final int mStrokeWidthInPixels;
        private final int mCornerRadiusInPixels;

        AssistantRectangleDrawable(@Nullable @ColorInt Integer backgroundColor,
                @Nullable @ColorInt Integer strokeColor, int strokeWidthInPixels,
                int cornerRadiusInPixels) {
            mBackgroundColor = backgroundColor;
            mStrokeColor = strokeColor;
            mStrokeWidthInPixels = strokeWidthInPixels;
            mCornerRadiusInPixels = cornerRadiusInPixels;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            GradientDrawable shape = new GradientDrawable();
            shape.setShape(GradientDrawable.RECTANGLE);
            shape.setCornerRadius(mCornerRadiusInPixels);
            if (mBackgroundColor != null) {
                shape.setColor(mBackgroundColor);
            }
            if (mStrokeColor != null) {
                shape.setStroke(mStrokeWidthInPixels, mStrokeColor);
            }
            callback.onResult(shape);
        }
    }

    private static class AssistantBitmapDrawable extends AssistantDrawable {
        private final ImageFetcher mImageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY);
        private final String mUrl;
        private final int mWidthInPixels;
        private final int mHeightInPixels;

        AssistantBitmapDrawable(String url, int width, int height) {
            mUrl = url;
            mWidthInPixels = width;
            mHeightInPixels = height;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            // TODO(b/143517837) Merge autofill assistant image fetcher UMA names.
            mImageFetcher.fetchImage(
                    mUrl, ImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME, result -> {
                        if (result != null) {
                            callback.onResult(new BitmapDrawable(context.getResources(),
                                    Bitmap.createScaledBitmap(
                                            result, mWidthInPixels, mHeightInPixels, true)));
                        } else {
                            callback.onResult(null);
                        }
                    });
        }
    }

    private static class AssistantResourceDrawable extends AssistantDrawable {
        private final String mResourceId;

        AssistantResourceDrawable(String resourceId) {
            mResourceId = resourceId;
        }

        @Override
        public void getDrawable(Context context, Callback<Drawable> callback) {
            int drawableId = context.getResources().getIdentifier(
                    mResourceId, "drawable", context.getPackageName());
            if (drawableId == 0) {
                callback.onResult(null);
            }
            try {
                callback.onResult(
                        ApiCompatibilityUtils.getDrawable(context.getResources(), drawableId));
            } catch (Resources.NotFoundException e) {
                callback.onResult(null);
            }
        }
    }
}
