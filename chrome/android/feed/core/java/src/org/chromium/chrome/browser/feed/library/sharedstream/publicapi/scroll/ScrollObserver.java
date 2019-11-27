// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll;

import android.view.View;

/** Observer which gets notified about scroll events. */
public interface ScrollObserver {
    /**
     * Callback with the new state of the scrollable container.
     *
     * @param view the view being scrolled.
     * @param featureId the id of the view where the scroll originated from; optional.
     * @param newState one of the RecyclerView SCROLLING_STATE_fields.
     * @param timestamp the time in ms when the scroll state change occurred.
     */
    default void onScrollStateChanged(View view, String featureId, int newState, long timestamp) {}

    /**
     * Callback with the change in x/y from the most recent scroll events.
     *
     * @param view the view being scrolled.
     * @param featureId the id of the view where the scroll originated from; optional.
     * @param dx the amount scrolled in the x direction.
     * @param dy the amount scrolled in the y direction.
     */
    void onScroll(View view, String featureId, int dx, int dy);
}
