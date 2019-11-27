// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.shadows;

import android.view.View;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowView;

/**
 * Robolectric Shadow for {@link View}.
 *
 * <p>Provides functionality to test view position.
 */
@Implements(View.class)
public class ExtendedShadowView extends ShadowView {
    private int mXLocation;
    private int mYLocation;
    private int mHeight;
    private int mWidth;
    private boolean mAttached;

    @Implementation
    public void getLocationOnScreen(int[] pos) {
        pos[0] = mXLocation;
        pos[1] = mYLocation;
    }

    public void setLocationOnScreen(int x, int y) {
        mXLocation = x;
        mYLocation = y;
    }

    @Implementation
    public void getLocationInWindow(int[] pos) {
        pos[0] = mXLocation;
        pos[1] = mYLocation;
    }

    public void setLocationInWindow(int x, int y) {
        mXLocation = x;
        mYLocation = y;
    }

    @Override
    @Implementation
    public boolean isAttachedToWindow() {
        return mAttached;
    }

    public void setAttachedToWindow(boolean attached) {
        this.mAttached = attached;
    }

    @Implementation
    public int getHeight() {
        return mHeight;
    }

    public void setHeight(int height) {
        this.mHeight = height;
    }

    @Implementation
    public int getWidth() {
        return mWidth;
    }

    public void setWidth(int width) {
        this.mWidth = width;
    }
}
