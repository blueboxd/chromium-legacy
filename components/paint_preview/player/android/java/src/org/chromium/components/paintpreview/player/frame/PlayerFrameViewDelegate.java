// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

/**
 * Used by {@link PlayerFrameView} to delegate view events to {@link PlayerFrameMediator}.
 */
interface PlayerFrameViewDelegate {
    /**
     * Called on layout with the attributed width and height.
     */
    void setLayoutDimensions(int width, int height);
    /**
     * Called when a scroll gesture is performed.
     * @param distanceX Horizontal scroll values in pixels.
     * @param distanceY Vertical scroll values in pixels.
     * @return Whether this scroll event was consumed.
     */
    boolean scrollBy(float distanceX, float distanceY);
    /**
     * Called when a scale gesture is performed.
     * @return Whether this scale event was consumed.
     */
    boolean scaleBy(float scaleFactor, float focalPointX, float focalPointY);

    /**
     * Called when a single tap gesture is performed.
     * @param x X coordinate of the point clicked.
     * @param y Y coordinate of the point clicked.
     */
    void onClick(int x, int y);

    /**
     * Called when a fling gesture is performed.
     * @param velocityX Horizontal velocity value in pixels.
     * @param velocityY Vertical velocity value in pixels.
     * @return Whether this fling was consumed.
     */
    boolean onFling(float velocityX, float velocityY);
}
