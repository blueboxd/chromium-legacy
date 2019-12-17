// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * Container view for omnibox suggestions made very specific for omnibox suggestions to minimize
 * any unnecessary measures and layouts.
 */
public class SuggestionView extends FrameLayout {
    private SuggestionViewDelegate mSuggestionDelegate;
    private TextView mTextLine1;
    private TextView mTextLine2;

    public SuggestionView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTextLine1 = findViewById(R.id.line_1);
        mTextLine2 = findViewById(R.id.line_2);
    }

    void setDelegate(SuggestionViewDelegate delegate) {
        mSuggestionDelegate = delegate;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final int height = getMeasuredHeight();
        final int line1Height = mTextLine1.getMeasuredHeight();
        final int line2Height =
                mTextLine2.getVisibility() == VISIBLE ? mTextLine2.getMeasuredHeight() : 0;

        // Center the text lines vertically.
        int line1VerticalOffset = Math.max(0, (height - line1Height - line2Height) / 2);
        // When one line is larger than the other, it contains extra vertical padding. This
        // produces more apparent whitespace above or below the text lines.  Add a small
        // offset to compensate.
        if (line1Height != line2Height) {
            line1VerticalOffset += (line2Height - line1Height) / 10;
        }

        int line2VerticalOffset = line1VerticalOffset + line1Height;
        int answerVerticalOffset = line2VerticalOffset;

        // The text line's total height is larger than this view, snap them to the top and
        // bottom of the view.
        if (line2VerticalOffset + line2Height > height) {
            line1VerticalOffset = 0;
            line2VerticalOffset = height - line2Height;
            answerVerticalOffset = line2VerticalOffset;
        }

        final int line1Top = top + line1VerticalOffset;
        final int line1Bottom = line1Top + line1Height;
        final int line2Top = top + line2VerticalOffset;
        final int line2Bottom = line2Top + line2Height;
        final int line1AdditionalStartPadding =
                mSuggestionDelegate.getAdditionalTextLine1StartPadding(mTextLine1, right - left);

        if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            int rightStartPos = right - left;
            mTextLine1.layout(
                    0, line1Top, rightStartPos - line1AdditionalStartPadding, line1Bottom);
            mTextLine2.layout(0, line2Top, rightStartPos, line2Bottom);
        } else {
            mTextLine1.layout(line1AdditionalStartPadding, line1Top, right - left, line1Bottom);
            mTextLine2.layout(0, line2Top, right - left, line2Bottom);
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int maxWidth = MeasureSpec.getSize(widthSpec);
        final int minimumHeight = getMinimumHeight();

        mTextLine1.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(minimumHeight, MeasureSpec.AT_MOST));
        mTextLine2.measure(MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(minimumHeight, MeasureSpec.AT_MOST));

        if (MeasureSpec.getMode(heightSpec) == MeasureSpec.EXACTLY) {
            super.onMeasure(widthSpec, heightSpec);
        } else {
            int desiredHeight = mTextLine1.getMeasuredHeight() + mTextLine2.getMeasuredHeight();
            int additionalPadding = getResources().getDimensionPixelSize(
                    R.dimen.omnibox_suggestion_text_vertical_padding);
            desiredHeight += additionalPadding;
            desiredHeight = Math.max(minimumHeight, desiredHeight);
            if (MeasureSpec.getMode(heightSpec) == MeasureSpec.AT_MOST) {
                desiredHeight = Math.min(desiredHeight, MeasureSpec.getSize(heightSpec));
            }

            super.onMeasure(
                    widthSpec, MeasureSpec.makeMeasureSpec(desiredHeight, MeasureSpec.EXACTLY));
        }
    }
}
