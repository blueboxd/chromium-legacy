// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.ui;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache.Corner;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache.RoundedCornerBitmaps;
import com.google.search.now.ui.piet.RoundedCornersProto.RoundedCorners.Corners;

/**
 * Rounding delegate for the bitmap masking strategy.
 *
 * <p>Handles creation of corner mask bitmaps. Since creating bitmaps is expensive, this class also
 * saves them and makes sure they are not re-created unless necessary. {@link
 * RoundedCornerWrapperView} decides which rounding strategy to use and sets the appropriate
 * delegate.
 */
class BitmapMaskingRoundedCornerDelegate extends RoundedCornerDelegate {
    private final Paint paint;
    private final Paint maskPaint;
    private final RoundedCornerMaskCache maskCache;
    private final int bitmask;
    private final Canvas offscreenCanvas;

    // Masks for each of the corners of the view; null if that corner is not rounded.
    /*@Nullable*/ private Bitmap cornerTL;
    /*@Nullable*/ private Bitmap cornerTR;
    /*@Nullable*/ private Bitmap cornerBL;
    /*@Nullable*/ private Bitmap cornerBR;

    // Keep track of current mask configuration so we can use cached values if nothing has changed.
    private int lastRadius = -1;
    private boolean lastRtL;

    BitmapMaskingRoundedCornerDelegate(
            RoundedCornerMaskCache maskCache, int bitmask, boolean isRtL) {
        this(maskCache, bitmask, isRtL, new Canvas());
    }

    BitmapMaskingRoundedCornerDelegate(
            RoundedCornerMaskCache maskCache, int bitmask, boolean isRtL, Canvas canvas) {
        this.maskCache = maskCache;
        offscreenCanvas = canvas;
        this.lastRtL = !isRtL; // Flip this so we must update the layout on the first time.
        this.bitmask = bitmask;
        this.paint = maskCache.getPaint();
        this.maskPaint = maskCache.getMaskPaint();
    }

    /**
     * Defensively make sure outline clipping is turned off, although that should be the default.
     */
    @Override
    public void initializeForView(ViewGroup view) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            view.setClipToOutline(false);
            view.setClipChildren(false);
        }
    }

    private void initCornerMasks(int radius, boolean isRtL) {
        if (radius < 1) {
            return;
        }
        RoundedCornerBitmaps masks = maskCache.getMasks(radius);

        if ((RoundedCornerViewHelper.shouldRoundCorner(Corners.TOP_START, bitmask) && !isRtL)
                || (RoundedCornerViewHelper.shouldRoundCorner(Corners.TOP_END, bitmask) && isRtL)) {
            cornerTL = masks.get(Corner.TOP_LEFT);
        } else {
            cornerTL = null;
        }

        if ((RoundedCornerViewHelper.shouldRoundCorner(Corners.TOP_END, bitmask) && !isRtL)
                || (RoundedCornerViewHelper.shouldRoundCorner(Corners.TOP_START, bitmask)
                        && isRtL)) {
            cornerTR = masks.get(Corner.TOP_RIGHT);
        } else {
            cornerTR = null;
        }

        if ((RoundedCornerViewHelper.shouldRoundCorner(Corners.BOTTOM_START, bitmask) && !isRtL)
                || (RoundedCornerViewHelper.shouldRoundCorner(Corners.BOTTOM_END, bitmask)
                        && isRtL)) {
            cornerBL = masks.get(Corner.BOTTOM_LEFT);
        } else {
            cornerBL = null;
        }

        if ((RoundedCornerViewHelper.shouldRoundCorner(Corners.BOTTOM_END, bitmask) && !isRtL)
                || (RoundedCornerViewHelper.shouldRoundCorner(Corners.BOTTOM_START, bitmask)
                        && isRtL)) {
            cornerBR = masks.get(Corner.BOTTOM_RIGHT);
        } else {
            cornerBR = null;
        }
    }

    /**
     * Creates corner masks (which cover the parts of the corners that should not be shown).
     *
     * <p>Masks must be created after the radius is known. However, if the size of the view and the
     * LtR remain the same, it will only create the masks once.
     */
    @Override
    public void onLayout(int radius, boolean isRtL, int width, int height) {
        if (radius == 0) {
            return;
        }
        if (radius == lastRadius && isRtL == lastRtL) {
            return;
        }

        initCornerMasks(radius, isRtL);

        lastRadius = radius;
        lastRtL = isRtL;
    }

    /**
     * Ensures that the wrapper view is invalidated when child views are invalidated.
     *
     * <p>This method only exists in Android O+.
     */
    @Override
    public void onDescendantInvalidated(View roundedCornerView, View invalidatedDescendant) {
        Rect targetRect = new Rect();
        invalidatedDescendant.getDrawingRect(targetRect);
        roundedCornerView.invalidate(targetRect);
    }

    /**
     * Ensures that the wrapper view is invalidated when the child view is.
     *
     * <p>This is only used in Android N- and is deprecated, but we must use it because
     * onDescendantInvalidated only exists in O+.
     */
    @Override
    public void invalidateChildInParent(View view, final Rect dirty) {
        view.invalidate(dirty);
    }

    /**
     * Creates a local bitmap and draws the content and corner masks on top of that.
     *
     * <p>The bitmap is drawn directly to the {@link Canvas} that is passed into this method.
     */
    @Override
    public void draw(RoundedCornerWrapperView view, Canvas canvas) {
        int width = view.getWidth();
        int height = view.getHeight();
        if (width == 0 || height == 0) {
            // The view is not visible, and offscreenBitmap creation will fail. Stop here. Call the
            // super method to make sure the lifecycle is handled properly.
            view.drawSuper(canvas);
            return;
        }
        int radius = view.getRadius(width, height);
        Bitmap localOffscreenBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        offscreenCanvas.setBitmap(localOffscreenBitmap);
        view.drawSuper(offscreenCanvas);
        drawWithCornerMasks(canvas, radius, width, height, localOffscreenBitmap);
    }

    private void drawWithCornerMasks(
            Canvas canvas, int radius, int width, int height, Bitmap offscreenBitmap) {
        // Crop the corners off using masks
        maskCorners(offscreenCanvas, width, height, radius);

        // Draw the offscreen bitmap (view with rounded corners) to the target canvas.
        canvas.drawBitmap(offscreenBitmap, 0f, 0f, paint);
    }

    /** Draws a mask on each corner that is rounded. */
    private void maskCorners(Canvas canvas, int width, int height, int radius) {
        if (cornerTL != null) {
            canvas.drawBitmap(cornerTL, 0, 0, maskPaint);
        }
        if (cornerTR != null) {
            canvas.drawBitmap(cornerTR, width - radius, 0, maskPaint);
        }
        if (cornerBL != null) {
            canvas.drawBitmap(cornerBL, 0, height - radius, maskPaint);
        }
        if (cornerBR != null) {
            canvas.drawBitmap(cornerBR, width - radius, height - radius, maskPaint);
        }
    }
}
