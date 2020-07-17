// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.chromium.url.GURL;

/**
 * Records metrics and handles player-wide (as opposed to per-frame) logic related to touch
 * gestures.
 */
public class PlayerGestureListener {
    private Runnable mUserInteractionCallback;
    private LinkClickHandler mLinkClickHandler;

    public PlayerGestureListener(
            LinkClickHandler linkClickHandler, Runnable userInteractionCallback) {
        mLinkClickHandler = linkClickHandler;
        mUserInteractionCallback = userInteractionCallback;
    }

    /**
     * Called when a tap gesture happens in the player.
     * @param url The GURL of the tapped link. If there are no links in the tapped region, this will
     *            be null.
     */
    public void onTap(GURL url) {
        if (url != null && mLinkClickHandler != null) {
            mLinkClickHandler.onLinkClicked(url);
            PlayerUserActionRecorder.recordLinkClick();
            return;
        }

        PlayerUserActionRecorder.recordUnconsumedTap();
    }

    public void onLongPress() {
        PlayerUserActionRecorder.recordLongPress();
    }

    public void onFling() {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        PlayerUserActionRecorder.recordFling();
    }

    public void onScroll() {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        PlayerUserActionRecorder.recordScroll();
    }

    public void onScale(boolean didFinish) {
        if (mUserInteractionCallback != null) mUserInteractionCallback.run();
        if (didFinish) PlayerUserActionRecorder.recordZoom();
    }
}
