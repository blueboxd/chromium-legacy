// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.view.View;

import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.ui.modelutil.PropertyModel;

class StatusIndicatorMediator implements ChromeFullscreenManager.FullscreenListener,
                                         StatusIndicatorCoordinator.StatusIndicatorObserver {
    private PropertyModel mModel;
    private ChromeFullscreenManager mFullscreenManager;
    private int mIndicatorHeight;

    private boolean mIsHiding;

    StatusIndicatorMediator(PropertyModel model, ChromeFullscreenManager fullscreenManager) {
        mModel = model;
        mFullscreenManager = fullscreenManager;
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        final boolean compositedVisible = topControlsMinHeightOffset > 0;
        // Composited view should be visible if we have a positive top min-height offset, or current
        // min-height.
        mModel.set(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, compositedVisible);

        // Android view should only be visible when the indicator is fully shown.
        mModel.set(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY,
                mIsHiding ? View.GONE
                          : (topControlsMinHeightOffset == mIndicatorHeight ? View.VISIBLE
                                                                            : View.INVISIBLE));

        final boolean doneHiding = !compositedVisible && mIsHiding;
        if (doneHiding) {
            mFullscreenManager.removeListener(this);
            mIsHiding = false;
        }
    }

    @Override
    public void onStatusIndicatorHeightChanged(int newHeight) {
        mIndicatorHeight = newHeight;

        if (mIndicatorHeight > 0) {
            mFullscreenManager.addListener(this);
            mIsHiding = false;
        } else {
            mIsHiding = true;
        }
    }
}
