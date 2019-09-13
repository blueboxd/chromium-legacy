// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ListView;
import android.widget.RelativeLayout;

import androidx.annotation.IdRes;

import org.chromium.chrome.R;

/**
 * {@link View} class for navigation sheet. Provided as content view for
 * {@link BottomSheet}.
 */
public class NavigationSheetView extends RelativeLayout {
    private final int mItemHeight;
    private final int mContentPadding;

    private ListView mListView;
    private int mEntryCount;

    public NavigationSheetView(Context context) {
        this(context, null);
    }

    public NavigationSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mItemHeight = getResources().getDimensionPixelSize(R.dimen.navigation_popup_item_height);
        mContentPadding = getSizePx(context, R.dimen.navigation_sheet_content_top_padding)
                + getSizePx(context, R.dimen.navigation_sheet_content_bottom_padding)
                + getSizePx(context, R.dimen.navigation_sheet_content_wrap_padding);
    }

    private static int getSizePx(Context context, @IdRes int id) {
        return context.getResources().getDimensionPixelSize(id);
    }

    /**
     * @return The vertical scroll offset of the list view containing the navigation history items.
     */
    int getVerticalScrollOffset() {
        View v = mListView.getChildAt(0);
        return v == null ? 0 : -(v.getTop() - mListView.getPaddingTop());
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mListView = findViewById(R.id.navigation_entries);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.EXACTLY) {
            int entryCount = mListView.getAdapter() != null ? mListView.getAdapter().getCount() : 0;

            // Makes the sheet height at most the half the screen height when there are
            // more items than it can show. The list then becomes scrollable.
            int height = Math.min(MeasureSpec.getSize(heightMeasureSpec) / 2 + mItemHeight / 2,
                    entryCount * mItemHeight + mContentPadding);
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
