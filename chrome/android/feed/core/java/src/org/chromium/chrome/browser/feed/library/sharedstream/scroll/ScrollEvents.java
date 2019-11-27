// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.sharedstream.scroll;

/** Events for scrolling. */
public interface ScrollEvents {
    /**
     * Notifies of the delta change of the scroll action and the ms timestamp of when the action
     * finished.
     */
    void onScrollEvent(int scrollAmount, long scrollEndTimestampMs);
}
