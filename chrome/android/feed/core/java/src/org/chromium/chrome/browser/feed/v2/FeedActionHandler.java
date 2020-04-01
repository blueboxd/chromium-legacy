// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

/**
 * Interface to handle the actions user can trigger on the feed.
 */
public interface FeedActionHandler {
    String KEY = "FeedActions";

    /**
     * Navigates the current tab to a particular URL.
     */
    void navigate(String url);

    /**
     * Navigates a new tab to a particular URL.
     */
    void navigateInNewTab(String url);

    /**
     * Requests additional content to be loaded. Once the load is completed, onStreamUpdated will be
     * called.
     */
    void loadMore();

    /**
     * Requests to dismiss a card. A change ID will be returned and it can be used to commit or
     * discard the change.
     */
    int requestDismissal();

    /**
     * Commits a previous requested dismissal denoted by change ID.
     */
    void commitDismissal(int changeId);

    /**
     * Discards a previous requested dismissal denoted by change ID.
     */
    void discardDismissal(int changeId);
}
