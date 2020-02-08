// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.view.View;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.fullscreen.FullscreenHtmlApiHandler.FullscreenHtmlApiDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages the basic fullscreen functionality required by a Tab.
 */
// TODO(tedchoc): Remove Tab's requirement on the fullscreen tokens to slim down the API of this
//                class.
public abstract class FullscreenManager {
    private final FullscreenHtmlApiHandler mHtmlApiHandler;
    @Nullable private Tab mTab;

    /**
     * Constructs the basic ChromeTab oriented FullscreenManager.
     *
     * @param window Top-level window to turn to fullscreen.
     */
    public FullscreenManager(Window window) {
        mHtmlApiHandler = new FullscreenHtmlApiHandler(window, createApiDelegate());
    }

    /**
     * @return The delegate that will handle the embedder specific requirements of the
     *         fullscreen API handler.
     */
    protected abstract FullscreenHtmlApiDelegate createApiDelegate();

    /**
     * @return The handler for managing interactions with the HTML fullscreen API.
     */
    protected FullscreenHtmlApiHandler getHtmlApiHandler() {
        return mHtmlApiHandler;
    }

    /**
     * @return The height of the top controls in pixels.
     */
    public abstract int getTopControlsHeight();

    /**
     * @return The minimum visible height top controls can have in pixels.
     */
    public abstract int getTopControlsMinHeight();

    /**
     * @return The offset of the controls from the top of the screen.
     */
    public abstract int getTopControlOffset();

    /**
     * @return The current top controls min-height. If the min-height is changing with an animation,
     * this will return a value between the old min-height and the new min-height, which is equal to
     * the current visible min-height. Otherwise, this will return the same value as
     * {@link #getTopControlsMinHeight()}.
     */
    public abstract int getTopControlsMinHeightOffset();

    /**
     * @return The height of the bottom controls in pixels.
     */
    public abstract int getBottomControlsHeight();

    /**
     * @return The minimum visible height bottom controls can have in pixels.
     */
    public abstract int getBottomControlsMinHeight();

    /**
     * @return The current bottom controls min-height. If the min-height is changing with an
     * animation, this will return a value between the old min-height and the new min-height, which
     * is equal to the current visible min-height. Otherwise, this will return the same value as
     * {@link #getBottomControlsMinHeight()}.
     */
    public abstract int getBottomControlsMinHeightOffset();

    /**
     * @return Whether or not the browser controls height changes should be animated.
     */
    public abstract boolean shouldAnimateBrowserControlsHeightChanges();

    /**
     * @return The offset of the controls from the bottom of the screen.
     */
    public abstract int getBottomControlOffset();

    /**
     * @return The ratio that the browser controls are off screen; this will be a number [0,1]
     *         where 1 is completely hidden and 0 is completely shown.
     */
    public abstract float getBrowserControlHiddenRatio();

    /**
     * @return The offset of the content from the top of the screen in px.
     */
    public abstract int getContentOffset();

    /**
     * Updates the positions of the browser controls and content to the default non fullscreen
     * values.
     */
    public abstract void setPositionsForTabToNonFullscreen();

    /**
     * Updates the positions of the browser controls and content based on the desired position of
     * the current tab.
     * @param topControlsOffset The Y offset of the top controls in px.
     * @param bottomControlsOffset The Y offset of the bottom controls in px.
     * @param topContentOffset The Y offset for the content in px.
     * @param topControlsMinHeightOffset The Y offset for the top controls min-height in px.
     * @param bottomControlsMinHeightOffset The Y offset for the bottom controls min-height in px.
     */
    public abstract void setPositionsForTab(int topControlsOffset, int bottomControlsOffset,
            int topContentOffset, int topControlsMinHeightOffset,
            int bottomControlsMinHeightOffset);

    /**
     * Updates the current ContentView's children and any popups with the correct offsets based on
     * the current fullscreen state.
     */
    public abstract void updateContentViewChildrenState();

    /**
     * Sets the currently selected tab for fullscreen.
     */
    public void setTab(@Nullable Tab tab) {
        if (mTab == tab) return;

        mTab = tab;
    }

    /**
     * @return The currently selected tab for fullscreen.
     */
    @Nullable public Tab getTab() {
        return mTab;
    }

    /**
     * Enters persistent fullscreen mode.  In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     */
    protected void enterPersistentFullscreenMode(FullscreenOptions options) {
        mHtmlApiHandler.enterPersistentFullscreenMode(options);
        updateMultiTouchZoomSupport(false);
    }

    /**
     * Exits persistent fullscreen mode.  In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     */
    public void exitPersistentFullscreenMode() {
        mHtmlApiHandler.exitPersistentFullscreenMode();
        updateMultiTouchZoomSupport(true);
    }

    /**
     * @see GestureListenerManager#updateMultiTouchZoomSupport(boolean).
     */
    protected void updateMultiTouchZoomSupport(boolean enable) {
        Tab tab = getTab();
        if (tab == null || tab.isHidden()) return;
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
            if (manager != null) manager.updateMultiTouchZoomSupport(enable);
        }
    }

    /**
     * @return Whether the application is in persistent fullscreen mode.
     * @see #setPersistentFullscreenMode(boolean)
     */
    public boolean getPersistentFullscreenMode() {
        return mHtmlApiHandler.getPersistentFullscreenMode();
    }

    /**
     * Notified when the system UI visibility for the current ContentView has changed.
     * @param visibility The updated UI visibility.
     * @see View#getSystemUiVisibility()
     */
    public void onContentViewSystemUiVisibilityChange(int visibility) {
        mHtmlApiHandler.onContentViewSystemUiVisibilityChange(visibility);
    }

    /**
     * Ensure the proper system UI flags are set after the window regains focus.
     * @see android.app.Activity#onWindowFocusChanged(boolean)
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        mHtmlApiHandler.onWindowFocusChanged(hasWindowFocus);
    }

    /**
     * Called when scrolling state of the ContentView changed.
     */
    public void onContentViewScrollingStateChanged(boolean scrolling) {}

    /**
     * Destroys the FullscreenManager
     */
    public void destroy() {
        setTab(null);
    }
}
