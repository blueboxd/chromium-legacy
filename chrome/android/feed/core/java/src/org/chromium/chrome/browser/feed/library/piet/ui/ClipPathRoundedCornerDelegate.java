// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.RectF;
import android.os.Build;
import android.view.ViewGroup;

/**
 * Rounding delegate for the clip path strategy.
 *
 * <p>Adds logic to create a path along the edge of the view and clip to it. {@link
 * RoundedCornerWrapperView} decides which rounding strategy to use and sets the appropriate
 * delegate.
 */
class ClipPathRoundedCornerDelegate extends RoundedCornerDelegate {
    private final Path mClipPath = new Path();

    /**
     * Turns off clipping to the outline.
     *
     * <p>This rounding strategy should not be doing outline clipping. This defensively makes sure
     * it's turned off, although that should be the default.
     */
    @Override
    public void initializeForView(ViewGroup view) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            view.setClipToOutline(false);
            view.setClipChildren(false);
        }
    }

    /** Updates the path every time the size changes. */
    @Override
    public void onSizeChanged(int radius, int width, int height, int bitmask, boolean isRtL) {
        float[] radii = RoundedCornerViewHelper.createRoundedCornerBitMask(radius, bitmask, isRtL);
        mClipPath.addRoundRect(new RectF(0, 0, width, height), radii, Path.Direction.CW);
    }

    /** Clips the path that was created in onSizeChanged(). */
    @Override
    public void dispatchDraw(Canvas canvas) {
        if (mClipPath != null) {
            canvas.clipPath(mClipPath);
        }
    }
}
