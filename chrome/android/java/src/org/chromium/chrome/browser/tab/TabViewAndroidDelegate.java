// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.Bundle;
import android.view.ViewGroup;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for Chrome.
 */
class TabViewAndroidDelegate extends ViewAndroidDelegate {
    /** Used for logging. */
    private static final String TAG = "TabVAD";

    private final Tab mTab;

    private int mPreviousTopControlsOffset;
    private int mPreviousBottomControlsOffset;
    private int mPreviousTopContentOffset;

    TabViewAndroidDelegate(Tab tab, ViewGroup containerView) {
        super(containerView);
        mTab = tab;
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mTab.onBackgroundColorChanged(color);
    }

    @Override
    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        mPreviousTopControlsOffset = topControlsOffsetY;
        mPreviousTopContentOffset = topContentOffsetY;
        TabBrowserControlsOffsetHelper.from(mTab).onOffsetsChanged(
                topControlsOffsetY, mPreviousBottomControlsOffset, topContentOffsetY);
    }

    @Override
    public void onBottomControlsChanged(int bottomControlsOffsetY, int bottomContentOffsetY) {
        mPreviousBottomControlsOffset = bottomControlsOffsetY;
        TabBrowserControlsOffsetHelper.from(mTab).onOffsetsChanged(
                mPreviousTopControlsOffset, bottomControlsOffsetY, mPreviousTopContentOffset);
    }

    @Override
    public int getSystemWindowInsetBottom() {
        ChromeActivity activity = mTab.getActivity();
        if (activity != null && activity.getInsetObserverView() != null) {
            return activity.getInsetObserverView().getSystemWindowInsetsBottom();
        }
        return 0;
    }

    @Override
    public void performPrivateImeCommand(String action, Bundle data) {
        AppHooks.get().performPrivateImeCommand(mTab.getWebContents(), action, data);
    }
}
