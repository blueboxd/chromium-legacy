// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

/** Object used to hold content data for logging events. */
public interface ContentLoggingData extends ElementLoggingData {
    /** Gets the time, in seconds from epoch, for when the content was published/made available. */
    long getPublishedTimeSeconds();

    /** Gets the score which was given to content from NowStream. */
    float getScore();

    /**
     * Gets offline availability status.
     *
     * <p>Note: The offline availability status for one piece of content can change. When this is
     * logged, it should be logged with the offline status as of logging time. This means that one
     * piece of content can emit multiple {@link ContentLoggingData} instances which are not equal
     * to each other.
     */
    boolean isAvailableOffline();
}
