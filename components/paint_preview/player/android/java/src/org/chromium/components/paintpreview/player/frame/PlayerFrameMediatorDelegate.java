// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

/**
 * API of the PlayerFrameMediator to helper classes.
 */
public interface PlayerFrameMediatorDelegate {
    /**
     * Triggers an update of the visual contents of the PlayerFrameView. This fetches updates the
     * model and fetches any new bitmaps asynchronously.
     * @param scaleChanged Indicates that the scale changed so all current bitmaps need to be
     *     discarded.
     */
    void updateVisuals(boolean scaleChanged);
}
