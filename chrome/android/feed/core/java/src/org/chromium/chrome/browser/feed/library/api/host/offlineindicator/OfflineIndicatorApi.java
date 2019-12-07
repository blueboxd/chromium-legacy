// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.offlineindicator;

import org.chromium.base.Consumer;

import java.util.List;

/** Api to allow the Feed to get information about offline availability status of content. */
public interface OfflineIndicatorApi {
    /**
     * Requests information on the offline status of content shown in the Feed.
     *
     * @param urlsToRetrieve list of urls we want to know about.
     * @param urlListConsumer subset of {@code urlsToRetrieve} which are available offline.
     */
    void getOfflineStatus(List<String> urlsToRetrieve, Consumer<List<String>> urlListConsumer);

    /** Adds a listener for changes to the offline availability of content. */
    void addOfflineStatusListener(OfflineStatusListener offlineStatusListener);

    /** Removes listener for changes the offline availability of content. */
    void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener);

    /** Listener for changes in the offline availability of content. */
    interface OfflineStatusListener {
        void updateOfflineStatus(String url, boolean availableOffline);
    }
}
