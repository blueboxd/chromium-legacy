// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.PaintDrawable;
import android.support.annotation.ColorInt;
import android.support.annotation.DrawableRes;
import android.support.v4.util.ObjectsCompat;
import android.support.v7.content.res.AppCompatResources;

import org.chromium.chrome.R;

/** Represents graphical decoration for the suggestion components. */
public class SuggestionDrawableState {
    private Drawable mDrawable;
    private boolean mAllowTint;
    private boolean mUseRoundedCorners;
    private boolean mIsLarge;

    /**
     * Create new SuggestionDrawableState representing a supplied Drawable object.
     *
     * @param cxt Current context.
     * @param drawable Drawable object to use.
     * @param isLarge Whether drawable object should be large (or small).
     * @param allowTint Whether supplied drawable can be tinted.
     * @param rounded Whether supplied drawable should use rounded corners.
     */
    public SuggestionDrawableState(
            Context ctx, Drawable drawable, boolean isLarge, boolean allowTint, boolean rounded) {
        mDrawable = drawable;
        mIsLarge = isLarge;
        mAllowTint = allowTint;
        mUseRoundedCorners = rounded;
    }

    /**
     * Create new SuggestionDrawableState representing a supplied Bitmap.
     * Bitmap drawables are not tintable.
     *
     * @param cxt Current context.
     * @param bitmap Bitmap to use.
     * @param isLarge Whether drawable object should be large (or small).
     * @param rounded Whether supplied drawable should use rounded corners.
     */
    public SuggestionDrawableState(Context ctx, Bitmap bitmap, boolean isLarge, boolean rounded) {
        this(ctx, new BitmapDrawable(bitmap), isLarge, false, rounded);
    }

    /**
     * Create new SuggestionDrawableState representing a supplied Color.
     * This instance is not tintable.
     * This method does not utilize ColorDrawables directly, because these are freely resize-able,
     * making it impossible to restrict its aspect ratio to a rectangle.
     *
     * @param cxt Current context.
     * @param color Color to show.
     * @param isLarge Whether drawable object should be large (or small).
     * @param rounded Whether supplied drawable should use rounded corners.
     */
    public SuggestionDrawableState(
            Context ctx, @ColorInt int color, boolean isLarge, boolean rounded) {
        this(ctx, new PaintDrawable(color), isLarge, false, rounded);
        final int edgeSize = ctx.getResources().getDimensionPixelSize(isLarge
                        ? R.dimen.omnibox_suggestion_36dp_icon_size
                        : R.dimen.omnibox_suggestion_24dp_icon_size);
        final PaintDrawable drawable = (PaintDrawable) mDrawable;
        drawable.setIntrinsicWidth(edgeSize);
        drawable.setIntrinsicHeight(edgeSize);
    }

    /**
     * Create new SuggestionDrawableState representing a supplied drawable resource.
     *
     * @param cxt Current context.
     * @param res Drawable resource to use.
     * @param isLarge Whether drawable object should be large (or small).
     * @param allowTint Whether supplied drawable can be tinted.
     * @param rounded Whether supplied drawable should use rounded corners.
     */
    public SuggestionDrawableState(Context ctx, @DrawableRes int res, boolean isLarge,
            boolean allowTint, boolean rounded) {
        this(ctx, AppCompatResources.getDrawable(ctx, res), isLarge, allowTint, rounded);
    }

    /** Get Drawable associated with this instance. */
    Drawable getDrawable() {
        return mDrawable;
    }

    /** Whether drawable can be tinted. */
    boolean isTintable() {
        return mAllowTint;
    }

    /** Whether drawable should be drawn as large. */
    boolean isLarge() {
        return mIsLarge;
    }

    /** Whether drawable should be rounded. */
    boolean isRounded() {
        return mUseRoundedCorners;
    }

    @Override
    public boolean equals(Object object) {
        if (this == object) return true;
        if (!(object instanceof SuggestionDrawableState)) return false;
        SuggestionDrawableState other = (SuggestionDrawableState) object;

        return mIsLarge == other.mIsLarge && mUseRoundedCorners == other.mUseRoundedCorners
                && mAllowTint == other.mAllowTint
                && ObjectsCompat.equals(mDrawable, other.mDrawable);
    }
};
