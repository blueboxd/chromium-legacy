// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

/**
 * Interface that extends {@link VisibilityListener} and provides additional callbacks for logging
 * events.
 */
public interface LoggingListener extends VisibilityListener {
    void onContentClicked();

    void onContentSwiped();

    /**
     * Called when the scroll state changes.
     *
     * @param newScrollState: the new {@link ScrollState} that is occurring (e.g. dragging, idle).
     */
    void onScrollStateChanged(int newScrollState);
}
