// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared;

import android.app.Activity;
import android.view.MotionEvent;

import org.chromium.chrome.browser.feed.StreamLifecycleManager;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream;

/**
 * The delegate of the {@link FeedSurfaceProvider} creator needs to implement.
 */
public interface FeedSurfaceDelegate {
    /**
     * Creates {@link StreamLifecycleManager} for the specified {@link Stream} in the {@link
     * Activity}.
     * @param stream The {@link Stream} managed by the {@link StreamLifecycleManager}.
     * @param activity The associated {@link Activity} of the {@link Stream}.
     * @return The {@link StreamLifecycleManager}.
     */
    StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity);

    /**
     * Checks whether the delegate want to intercept the given touch event.
     * @param ev The given {@link MotioneEvent}
     * @return True if the delegate want to intercept the event, otherwise return false.
     */
    boolean onInterceptTouchEvent(MotionEvent ev);
}
