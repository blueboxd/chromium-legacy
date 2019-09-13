// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

/**
 * The coordinator for a status indicator that is positioned below the status bar and is persistent.
 * Typically used to relay status, e.g. indicate user is offline.
 */
public class StatusIndicatorCoordinator {
    private StatusIndicatorSceneLayer mSceneLayer;

    public StatusIndicatorCoordinator() {
        mSceneLayer = new StatusIndicatorSceneLayer();
    }

    /**
     * Change the visibility of the status indicator.
     * @param visible True if visible.
     */
    public void setIsVisible(boolean visible) {
        assert mSceneLayer != null;
        mSceneLayer.setIsVisible(visible);
    }

    /**
     * Is the status indicator currently visible.
     * @return True if visible.
     */
    public boolean isVisible() {
        return mSceneLayer.isSceneOverlayTreeShowing();
    }

    /**
     * @return The {@link StatusIndicatorSceneLayer}.
     */
    public StatusIndicatorSceneLayer getSceneLayer() {
        return mSceneLayer;
    }
}
