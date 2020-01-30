// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;

/**
 * The container that holds both infobars and snackbars. It will be translated up and down when the
 * bottom controls' offset changes.
 */
public class BottomContainer
        extends FrameLayout implements FullscreenListener, CompositorViewResizer.Observer {
    /** The {@link ChromeFullscreenManager} to listen for controls offset changes. */
    private ChromeFullscreenManager mFullscreenManager;

    /** A {@link CompositorViewResizer} to listen to for keyboard extension size changes. */
    private CompositorViewResizer mKeyboardExtensionSizeManager;

    /** The desired Y offset if unaffected by other UI. */
    private float mBaseYOffset;

    /**
     * Constructor for XML inflation.
     */
    public BottomContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes this container.
     */
    public void initialize(ChromeFullscreenManager fullscreenManager,
            CompositorViewResizer keyboardExtensionSizeManager) {
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);
        mKeyboardExtensionSizeManager = keyboardExtensionSizeManager;
        mKeyboardExtensionSizeManager.addObserver(this);
        setTranslationY(mBaseYOffset);
    }

    // CompositorViewResizer methods
    @Override
    public void onHeightChanged(int keyboardHeight) {
        setTranslationY(mBaseYOffset);
    }

    // FullscreenListener methods
    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        setTranslationY(mBaseYOffset);
    }

    @Override
    public void setTranslationY(float y) {
        mBaseYOffset = y;

        float offsetFromControls = mFullscreenManager.getBottomControlOffset()
                - mFullscreenManager.getBottomControlsHeight();
        offsetFromControls -= mKeyboardExtensionSizeManager.getHeight();

        // Sit on top of either the bottom sheet or the bottom toolbar depending on which is larger
        // (offsets are negative).
        super.setTranslationY(mBaseYOffset + offsetFromControls);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        setTranslationY(mBaseYOffset);
    }

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}
}
