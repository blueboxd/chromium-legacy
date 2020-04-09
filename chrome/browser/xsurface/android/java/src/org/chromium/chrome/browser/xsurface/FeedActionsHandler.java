// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Interface to provide chromium calling points for a feed.
 */
public interface FeedActionsHandler {
    String KEY = "FeedActions";

    /**
     * Requests additional content to be loaded. Once the load is completed, onStreamUpdated will be
     * called.
     */
    default void loadMore() {}

    /**
     * Sends data back to the server when content is clicked.
     */
    default void processThereAndBackAgainData(byte[] data) {}

    /**
     * Requests to dismiss a card. A change ID will be returned and it can be used to commit or
     * discard the change.
     */
    default int requestDismissal() {
        return 0;
    }

    /**
     * Commits a previous requested dismissal denoted by change ID.
     */
    default void commitDismissal(int changeId) {}

    /**
     * Discards a previous requested dismissal denoted by change ID.
     */
    default void discardDismissal(int changeId) {}
}
