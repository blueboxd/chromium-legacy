// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListAdapter;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.feed.R;

import java.util.ArrayList;
import java.util.List;

/** Makes measurements for a Menu based on an {@link ArrayAdapter}. */
public class MenuMeasurer {
    public static final int NO_MAX_HEIGHT = Integer.MAX_VALUE;
    public static final int NO_MAX_WIDTH = Integer.MAX_VALUE;
    private static final String TAG = "MenuMeasurer";

    private final Context mContext;

    public MenuMeasurer(Context context) {
        this.mContext = context;
    }

    // TODO: Test measureAdapterContent fully instead of just calculateSize.
    public Size measureAdapterContent(
            ViewGroup parent, ListAdapter adapter, int windowPadding, int maxWidth, int maxHeight) {
        return calculateSize(getMeasurements(parent, adapter), windowPadding, maxWidth, maxHeight);
    }

    private List<Size> getMeasurements(ViewGroup parent, ListAdapter adapter) {
        ArrayList<Size> measurements = new ArrayList<>(adapter.getCount());
        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        View view = null;
        for (int i = 0; i < adapter.getCount(); i++) {
            view = getViewFromAdapter(adapter, i, view, parent);
            view.measure(widthMeasureSpec, heightMeasureSpec);
            measurements.add(new Size(view.getMeasuredWidth(), view.getMeasuredHeight()));
        }
        return measurements;
    }

    @VisibleForTesting
    Size calculateSize(List<Size> measuredSizes, int windowPadding, int maxWidth, int maxHeight) {
        int largestWidth = 0;
        int totalHeight = 0;
        for (Size size : measuredSizes) {
            int itemWidth = size.getWidth();
            totalHeight += size.getHeight();

            if (itemWidth > largestWidth) {
                largestWidth = itemWidth;
            }
        }

        int widthUnit = mContext.getResources().getDimensionPixelSize(R.dimen.menu_width_multiple);

        int width = Math.min(roundLargestPopupContentWidth(largestWidth, widthUnit),
                maxWidth - windowPadding - windowPadding);
        return new Size(width, Math.min(totalHeight + windowPadding + windowPadding, maxHeight));
    }

    /**
     * Given the {@code largestWidth} of members of popup content and the {@code widthUnit}, returns
     * the smallest multiple of {@code widthUnit} that is at least as large as {@code largestWidth}.
     */
    private int roundLargestPopupContentWidth(int largestWidth, int widthUnit) {
        return Math.round((((float) largestWidth / (float) widthUnit) + 0.5f)) * widthUnit;
    }

    /** Minimal method to suppress an incorrect nullness error. */
    @SuppressWarnings("nullness:argument.type.incompatible")
    private View getViewFromAdapter(
            ListAdapter arrayAdapter, int index, @Nullable View convertView, ViewGroup parentView) {
        return arrayAdapter.getView(index, convertView, parentView);
    }
}
