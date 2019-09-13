// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import org.chromium.chrome.touchless.R;

/** ImageView that keeps its image square and limited to a static set of sizes. */
public class QuantizedSizeIconView extends ImageView {
    private int[] mSizes;

    public QuantizedSizeIconView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        Resources res = context.getResources();
        TypedArray styledAttrs =
                context.obtainStyledAttributes(attrs, R.styleable.QuantizedSizeIconView);
        // Defaults are MAX_VALUE, in other words do not quantize.
        mSizes = new int[] {styledAttrs.getDimensionPixelOffset(
                                    R.styleable.QuantizedSizeIconView_largeSize, Integer.MAX_VALUE),
                styledAttrs.getDimensionPixelOffset(
                        R.styleable.QuantizedSizeIconView_smallSize, Integer.MAX_VALUE)};
        styledAttrs.recycle();
    }

    public void setSizes(int[] sizes) {
        mSizes = sizes;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int widthSpecMode = MeasureSpec.getMode(widthMeasureSpec);
        int heightSpecMode = MeasureSpec.getMode(heightMeasureSpec);

        int measuredWidth = MeasureSpec.getSize(widthMeasureSpec);
        int measuredHeight = MeasureSpec.getSize(heightMeasureSpec);

        if (widthSpecMode == MeasureSpec.UNSPECIFIED && heightSpecMode != MeasureSpec.UNSPECIFIED) {
            measuredWidth = measuredHeight;
        } else if (heightSpecMode == MeasureSpec.UNSPECIFIED
                && widthSpecMode != MeasureSpec.UNSPECIFIED) {
            measuredHeight = measuredWidth;
        } else if (heightSpecMode == MeasureSpec.UNSPECIFIED) {
            // If both are UNSPECIFIED, take up as much room as possible.
            measuredWidth = Integer.MAX_VALUE;
            measuredHeight = Integer.MAX_VALUE;
        } else if (widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode == MeasureSpec.EXACTLY) {
            measuredWidth = Math.min(measuredHeight, measuredWidth);
        } else if (heightSpecMode == MeasureSpec.AT_MOST && widthSpecMode == MeasureSpec.EXACTLY) {
            measuredHeight = Math.min(measuredWidth, measuredHeight);
        } else if (widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode == MeasureSpec.AT_MOST) {
            int minimumDimension = Math.min(measuredHeight, measuredWidth);
            measuredWidth = minimumDimension;
            measuredHeight = minimumDimension;
        }
        // else keep values from the MeasureSpec because both modes are EXACTLY.

        measuredHeight = quantizeDimension(measuredHeight);
        measuredWidth = quantizeDimension(measuredWidth);

        setMeasuredDimension(measuredWidth, measuredHeight);
    }

    private int quantizeDimension(int dimension) {
        for (int sizeOption : mSizes) {
            if (sizeOption <= dimension) {
                return sizeOption;
            }
        }
        return dimension;
    }
}
