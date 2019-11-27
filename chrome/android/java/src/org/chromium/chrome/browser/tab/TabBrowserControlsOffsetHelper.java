// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.UserData;

/**
 * Helper that coordinates the browser controls offsets from the perspective of a particular Tab.
 */
public class TabBrowserControlsOffsetHelper extends EmptyTabObserver implements UserData {
    private static final Class<TabBrowserControlsOffsetHelper> USER_DATA_KEY =
            TabBrowserControlsOffsetHelper.class;

    private final TabImpl mTab;

    private int mTopControlsOffset;
    private int mBottomControlsOffset;
    private int mContentOffset;

    /** {@code true} if offset was changed by compositor. */
    private boolean mOffsetInitialized;

    /**
     * Get (or lazily create) the offset helper for a particular Tab.
     * @param tab The tab whose helper is being retrieved.
     * @return The offset helper for a given tab.
     */
    @NonNull
    public static TabBrowserControlsOffsetHelper get(Tab tab) {
        TabBrowserControlsOffsetHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = new TabBrowserControlsOffsetHelper(tab);
            tab.getUserDataHost().setUserData(USER_DATA_KEY, helper);
        }
        return helper;
    }

    private TabBrowserControlsOffsetHelper(Tab tab) {
        mTab = (TabImpl) tab;
        tab.addObserver(this);
    }

    /**
     * Sets new top control and content offset from renderer.
     * @param topControlsOffset Top control offset.
     * @param contentOffset Content offset.
     */
    void setTopOffset(int topControlsOffset, int contentOffset) {
        if (mOffsetInitialized && topControlsOffset == mTopControlsOffset
                && mContentOffset == contentOffset) {
            return;
        }
        mTopControlsOffset = topControlsOffset;
        mContentOffset = contentOffset;
        notifyControlsOffsetChanged();
    }

    /**
     * Sets new bottom control offset from renderer.
     * @param bottomControlsOffset Bottom control offset.
     */
    void setBottomOffset(int bottomControlsOffset) {
        if (mOffsetInitialized && mBottomControlsOffset == bottomControlsOffset) return;
        mBottomControlsOffset = bottomControlsOffset;
        notifyControlsOffsetChanged();
    }

    private void notifyControlsOffsetChanged() {
        mOffsetInitialized = true;
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onBrowserControlsOffsetChanged(
                    mTab, mTopControlsOffset, mBottomControlsOffset, mContentOffset);
        }
    }

    @Override
    public void onCrash(Tab tab) {
        super.onCrash(tab);
        mTopControlsOffset = 0;
        mBottomControlsOffset = 0;
        mContentOffset = 0;
        mOffsetInitialized = false;
    }

    /** @return Top control offset */
    public int topControlsOffset() {
        return mTopControlsOffset;
    }

    /** @return Bottom control offset */
    public int bottomControlsOffset() {
        return mBottomControlsOffset;
    }

    /** @return content offset */
    public int contentOffset() {
        return mContentOffset;
    }

    /** @return Whether the control offset is initialized by compositor. */
    public boolean offsetInitialized() {
        return mOffsetInitialized;
    }
}
