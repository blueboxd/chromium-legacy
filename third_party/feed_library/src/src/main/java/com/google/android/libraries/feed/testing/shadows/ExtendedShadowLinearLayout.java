// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.shadows;

import android.widget.LinearLayout;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLinearLayout;

/**
 * Robolectric Shadow for {@link LinearLayout}.
 *
 * <p>Provides functionality to test view position.
 */
@Implements(LinearLayout.class)
public class ExtendedShadowLinearLayout extends ShadowLinearLayout {
    private int xLocation;
    private int yLocation;
    private int height;
    private int width;
    private boolean attached;

    @Implementation
    public void getLocationOnScreen(int[] pos) {
        pos[0] = xLocation;
        pos[1] = yLocation;
    }

    public void setLocationOnScreen(int x, int y) {
        xLocation = x;
        yLocation = y;
    }

    @Implementation
    public void getLocationInWindow(int[] pos) {
        pos[0] = xLocation;
        pos[1] = yLocation;
    }

    public void setLocationInWindow(int x, int y) {
        xLocation = x;
        yLocation = y;
    }

    @Override
    @Implementation
    public boolean isAttachedToWindow() {
        return attached;
    }

    public void setAttachedToWindow(boolean attached) {
        this.attached = attached;
    }

    @Implementation
    public int getHeight() {
        return height;
    }

    public void setHeight(int height) {
        this.height = height;
    }

    @Implementation
    public int getWidth() {
        return width;
    }

    public void setWidth(int width) {
        this.width = width;
    }
}
