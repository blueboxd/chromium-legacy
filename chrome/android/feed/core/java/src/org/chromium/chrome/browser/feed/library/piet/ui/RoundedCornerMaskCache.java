// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.util.LruCache;

/** Caches rounded corner masks to save memory and time spent creating them */
public class RoundedCornerMaskCache {
    // TODO: Make cache size configurable.
    private static final int CACHE_SIZE = 8;

    @IntDef({Corner.TOP_LEFT, Corner.TOP_RIGHT, Corner.BOTTOM_LEFT, Corner.BOTTOM_RIGHT})
    @interface Corner {
        int TOP_LEFT = 0;
        int TOP_RIGHT = 1;
        int BOTTOM_LEFT = 2;
        int BOTTOM_RIGHT = 3;
    }

    private final Canvas mCanvas;
    private final Paint mPaint;
    private final Paint mMaskPaint;
    private final LruCache<Integer, RoundedCornerBitmaps> mCache;

    public RoundedCornerMaskCache() {
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mMaskPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        mMaskPaint.setXfermode(new PorterDuffXfermode(Mode.CLEAR));

        mCanvas = new Canvas();
        mCache = new LruCache<Integer, RoundedCornerBitmaps>(CACHE_SIZE) {
            @Override
            protected RoundedCornerBitmaps create(Integer key) {
                return new RoundedCornerBitmaps(key, mCanvas, mMaskPaint);
            }
        };
    }

    Paint getPaint() {
        return mPaint;
    }

    Paint getMaskPaint() {
        return mMaskPaint;
    }

    RoundedCornerBitmaps getMasks(int radius) {
        RoundedCornerBitmaps masks = mCache.get(radius);
        if (masks == null) {
            masks = new RoundedCornerBitmaps(radius, mCanvas, mMaskPaint);
            mCache.put(radius, masks);
        }
        return masks;
    }

    public void purge() {
        mCache.evictAll();
    }

    static class RoundedCornerBitmaps {
        @VisibleForTesting
        final Bitmap[] mMasks;
        private final Canvas mCanvas;
        private final int mRadius;
        private final Paint mMaskPaint;

        RoundedCornerBitmaps(int radius, Canvas canvas, Paint maskPaint) {
            mMasks = new Bitmap[4];
            this.mCanvas = canvas;
            this.mRadius = radius;
            this.mMaskPaint = maskPaint;
        }

        Bitmap get(@Corner int corner) {
            switch (corner) {
                case Corner.TOP_LEFT:
                    if (mMasks[Corner.TOP_LEFT] == null) {
                        mMasks[Corner.TOP_LEFT] =
                                Bitmap.createBitmap(mRadius, mRadius, Config.ALPHA_8);
                        mCanvas.setBitmap(mMasks[Corner.TOP_LEFT]);
                        mCanvas.drawColor(Color.BLACK);
                        mCanvas.drawCircle(mRadius, mRadius, mRadius, mMaskPaint);
                    }
                    return mMasks[Corner.TOP_LEFT];

                case Corner.TOP_RIGHT:
                    if (mMasks[Corner.TOP_RIGHT] == null) {
                        mMasks[Corner.TOP_RIGHT] =
                                Bitmap.createBitmap(mRadius, mRadius, Config.ALPHA_8);
                        mCanvas.setBitmap(mMasks[Corner.TOP_RIGHT]);
                        mCanvas.drawColor(Color.BLACK);
                        mCanvas.drawCircle(0, mRadius, mRadius, mMaskPaint);
                    }
                    return mMasks[Corner.TOP_RIGHT];

                case Corner.BOTTOM_LEFT:
                    if (mMasks[Corner.BOTTOM_LEFT] == null) {
                        mMasks[Corner.BOTTOM_LEFT] =
                                Bitmap.createBitmap(mRadius, mRadius, Config.ALPHA_8);
                        mCanvas.setBitmap(mMasks[Corner.BOTTOM_LEFT]);
                        mCanvas.drawColor(Color.BLACK);
                        mCanvas.drawCircle(mRadius, 0, mRadius, mMaskPaint);
                    }
                    return mMasks[Corner.BOTTOM_LEFT];

                case Corner.BOTTOM_RIGHT:
                    if (mMasks[Corner.BOTTOM_RIGHT] == null) {
                        mMasks[Corner.BOTTOM_RIGHT] =
                                Bitmap.createBitmap(mRadius, mRadius, Config.ALPHA_8);
                        mCanvas.setBitmap(mMasks[Corner.BOTTOM_RIGHT]);
                        mCanvas.drawColor(Color.BLACK);
                        mCanvas.drawCircle(0, 0, mRadius, mMaskPaint);
                    }
                    return mMasks[Corner.BOTTOM_RIGHT];

                default:
                    throw new IllegalArgumentException(
                            String.format("Unrecognized corner: %s", corner));
            }
        }
    }
}
