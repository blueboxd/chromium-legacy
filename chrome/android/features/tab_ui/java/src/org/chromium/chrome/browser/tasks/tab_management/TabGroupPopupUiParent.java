// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Parent for TabGroupPopUi component.
 */
public class TabGroupPopupUiParent {
    private final Context mContext;
    private final float mIconSize;

    private AnchoredPopupWindow mTopPopupWindow;
    private ViewGroup mTopStripContainerView;
    private AnchoredPopupWindow mBottomPopupWindow;
    private ViewGroup mBottomStripContainerView;
    private AnchoredPopupWindow mCurrentPopupWindow;
    private ViewGroup mCurrentStripContainerView;
    private boolean mIsShowingTopToolbar;

    TabGroupPopupUiParent(Context context, View anchorView) {
        mContext = context;
        mIconSize = context.getResources().getDimension(R.dimen.infobar_big_icon_size);
        onAnchorViewChanged(anchorView, anchorView.getId());
    }

    void onAnchorViewChanged(View anchorView, int anchorViewId) {
        ViewGroup previousContainerView = mCurrentStripContainerView;
        mIsShowingTopToolbar = anchorViewId == R.id.toolbar;

        if (mIsShowingTopToolbar) {
            if (mTopPopupWindow == null) {
                mTopStripContainerView = new FrameLayout(mContext);
                mTopPopupWindow = createTabGroupPopUi(anchorView, true);
            }
            mCurrentPopupWindow = mTopPopupWindow;
            mCurrentStripContainerView = mTopStripContainerView;
        } else {
            if (mBottomPopupWindow == null) {
                mBottomStripContainerView = new FrameLayout(mContext);
                mBottomPopupWindow = createTabGroupPopUi(anchorView, false);
            }
            mCurrentPopupWindow = mBottomPopupWindow;
            mCurrentStripContainerView = mBottomStripContainerView;
        }

        if (previousContainerView == null) return;
        // Transfer the ownership of the tab strip view.
        View contentView = previousContainerView.getChildAt(0);
        assert contentView != null;
        previousContainerView.removeViewAt(0);
        mCurrentStripContainerView.addView(contentView);
    }

    private AnchoredPopupWindow createTabGroupPopUi(View anchorView, boolean isTop) {
        ViewGroup stripContainerView = isTop ? mTopStripContainerView : mBottomStripContainerView;
        assert stripContainerView != null;
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

        AnchoredPopupWindow popupWindow = new AnchoredPopupWindow(mContext, anchorView,
                ApiCompatibilityUtils.getDrawable(
                        mContext.getResources(), R.drawable.popup_bg_tinted),
                stripContainerView, rectProvider) {
            @Override
            public void onRectHidden() {}
        };
        popupWindow.setHorizontalOverlapAnchor(true);
        popupWindow.setOutsideTouchable(true);
        popupWindow.setDismissOnTouchInteraction(false);
        popupWindow.setFocusable(false);
        popupWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        popupWindow.setPreferredVerticalOrientation(isTop
                        ? AnchoredPopupWindow.VerticalOrientation.BELOW
                        : AnchoredPopupWindow.VerticalOrientation.ABOVE);
        stripContainerView.setBackground(ApiCompatibilityUtils.getDrawable(
                mContext.getResources(), R.drawable.popup_bg_tinted));

        if (isTop) {
            rectProvider.setInsetPx(0, (int) mIconSize, 0, (int) mIconSize);
        } else {
            rectProvider.setInsetPx(0, (int) mIconSize / 2, 0, (int) mIconSize / 2);
        }
        return popupWindow;
    }

    void updateStripWindow() {
        if (mCurrentPopupWindow == null) return;
        Handler handler = new Handler();
        handler.post(() -> mCurrentPopupWindow.onRectChanged());
    }

    void setVisibility(boolean isVisible) {
        if (isVisible) {
            mCurrentPopupWindow.show();
        } else {
            mCurrentPopupWindow.dismiss();
        }
    }

    ViewGroup getCurrentContainerView() {
        assert mCurrentStripContainerView != null;
        return mCurrentStripContainerView;
    }

    void setContentViewAlpha(float alpha) {
        if (mCurrentStripContainerView == null) return;
        mCurrentStripContainerView.setAlpha(alpha);
    }

    @VisibleForTesting
    AnchoredPopupWindow getCurrentPopupWindowForTesting() {
        return mCurrentPopupWindow;
    }

    @VisibleForTesting
    ViewGroup getCurrentStripContainerViewForTesting() {
        return getCurrentContainerView();
    }

    @VisibleForTesting
    ViewGroup getTopStripContainerViewForTesting() {
        return mTopStripContainerView;
    }

    @VisibleForTesting
    ViewGroup getBottomStripContainerViewForTesting() {
        return mBottomStripContainerView;
    }
}