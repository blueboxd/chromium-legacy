// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;

/**
 * A special linear layout that limits its maximum size to always stay below the Chrome navigation
 * bar.
 */
public class AssistantRootViewContainer
        extends LinearLayout implements ChromeFullscreenManager.FullscreenListener {
    private final ChromeActivity mActivity;
    private final ChromeFullscreenManager mFullscreenManager;
    private Rect mVisibleViewportRect = new Rect();

    public AssistantRootViewContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        assert context instanceof ChromeActivity;
        mActivity = (ChromeActivity) context;
        mFullscreenManager = mActivity.getFullscreenManager();
        mActivity.getFullscreenManager().addListener(this);
    }

    @Override
    public void onContentOffsetChanged(int offset) {
        invalidate();
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {
        invalidate();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        invalidate();
    }

    void destroy() {
        mFullscreenManager.removeListener(this);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
        super.onMeasure(widthMeasureSpec,
                MeasureSpec.makeMeasureSpec(
                        Math.min(MeasureSpec.getSize(heightMeasureSpec),
                                mVisibleViewportRect.height()
                                        - mFullscreenManager.getContentOffset()
                                        - mFullscreenManager.getBottomControlsHeight()
                                        - mFullscreenManager.getBottomControlOffset()),
                        MeasureSpec.AT_MOST));
    }
}
