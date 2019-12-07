// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import org.chromium.base.Consumer;

/**
 * Handles loading images from the host. In particular, handles the resizing of images as well as
 * the fading animation.
 */
public class LoadImageCallback implements Consumer</*@Nullable*/ Drawable> {
    static final int FADE_IN_ANIMATION_TIME_MS = 300;

    private final ImageView mImageView;
    private final ScaleType mScaleType;
    private final long mInitialTime;
    /*@Nullable*/ private final Integer mOverlayColor;
    private final boolean mFadeImage;
    private final AdapterParameters mParameters;

    private boolean mCancelled;

    /*@Nullable*/ private Drawable mFinalDrawable;

    LoadImageCallback(ImageView imageView, ScaleType scaleType,
            /*@Nullable*/ Integer overlayColor, boolean fadeImage, AdapterParameters parameters,
            FrameContext frameContext) {
        this.mImageView = imageView;
        this.mScaleType = scaleType;
        this.mOverlayColor = overlayColor;
        this.mFadeImage = fadeImage;
        this.mParameters = parameters;
        this.mInitialTime = parameters.mClock.elapsedRealtime();
    }

    @Override
    public void accept(/*@Nullable*/ Drawable drawable) {
        if (mCancelled || drawable == null) {
            return;
        }

        mImageView.setScaleType(mScaleType);

        final Drawable localDrawable = ViewUtils.applyOverlayColor(drawable, mOverlayColor);
        this.mFinalDrawable = localDrawable;

        // If we are in the process of binding when we get the image, we should not fade in the
        // image as the image was cached.
        if (!shouldFadeInImage()) {
            mImageView.setImageDrawable(localDrawable);
            // Invalidating the view as the view doesn't update if not manually updated here.
            mImageView.invalidate();
            return;
        }

        Drawable initialDrawable = mImageView.getDrawable() != null
                ? mImageView.getDrawable()
                : new ColorDrawable(Color.TRANSPARENT);

        TransitionDrawable transitionDrawable =
                new TransitionDrawable(new Drawable[] {initialDrawable, localDrawable});
        mImageView.setImageDrawable(transitionDrawable);
        transitionDrawable.setCrossFadeEnabled(true);
        transitionDrawable.startTransition(FADE_IN_ANIMATION_TIME_MS);

        mImageView.postDelayed(() -> {
            if (mCancelled) {
                return;
            }
            // Allows GC of the initial drawable and the transition drawable. Additionally
            // fixes the issue where the transition sometimes doesn't occur, which would
            // result in blank images.
            mImageView.setImageDrawable(this.mFinalDrawable);
        }, FADE_IN_ANIMATION_TIME_MS);
    }

    private boolean shouldFadeInImage() {
        return mFadeImage
                && (mParameters.mClock.elapsedRealtime() - mInitialTime)
                > mParameters.mHostProviders.getAssetProvider().getFadeImageThresholdMs();
    }

    void cancel() {
        this.mCancelled = true;
    }
}
