// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.publicapi.scroll;

/** Interface to register for scroll events. */
public interface ScrollObservable {
    /** Adds an observer for listening to future scroll events. */
    void addScrollObserver(ScrollObserver observer);

    /** Removes the specified observer from listening to scroll events. */
    void removeScrollObserver(ScrollObserver observer);

    /** Provides the current scroll state of the monitor. */
    int getCurrentScrollState();
}
