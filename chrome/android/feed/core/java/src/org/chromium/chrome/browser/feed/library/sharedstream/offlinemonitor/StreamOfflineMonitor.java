// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi.OfflineStatusListener;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Monitors and notifies consumers as to when the offline status of given urls change. */
public class StreamOfflineMonitor implements OfflineStatusListener {
    private static final String TAG = "StreamOfflineMonitor";

    private final Set<String> mContentToRequestStatus = new HashSet<>();
    private final Set<String> mOfflineContent = new HashSet<>();
    private final OfflineIndicatorApi mOfflineIndicatorApi;

    @VisibleForTesting
    protected final Map<String, List<Consumer<Boolean>>> mOfflineStatusConsumersMap =
            new HashMap<>();

    @SuppressWarnings("nullness:argument.type.incompatible")
    public StreamOfflineMonitor(OfflineIndicatorApi offlineIndicatorApi) {
        offlineIndicatorApi.addOfflineStatusListener(this);
        this.mOfflineIndicatorApi = offlineIndicatorApi;
    }

    /**
     * Returns whether the given url is known to be available offline.
     *
     * <p>Note: As this monitor does not have complete knowledge as to what is available offline,
     * this may return {@code false} for content that is available offline. Once the host has been
     * informed that such articles are on the feed, it will eventually inform the feed of any
     * additional offline stories. At that point, any relevant listeners will be notified.
     */
    public boolean isAvailableOffline(String url) {
        if (mOfflineContent.contains(url)) {
            return true;
        }
        mContentToRequestStatus.add(url);
        return false;
    }

    /**
     * Adds consumer for any changes in the status of the offline availability of the given story.
     * Only triggered on change, not immediately upon registering.
     */
    public void addOfflineStatusConsumer(String url, Consumer<Boolean> isOfflineConsumer) {
        if (!mOfflineStatusConsumersMap.containsKey(url)) {
            // Initializing size of lists to 1 as it is unlikely that we would have more than one
            // listener per URL.
            mOfflineStatusConsumersMap.put(url, new ArrayList<>(1));
        }

        mOfflineStatusConsumersMap.get(url).add(isOfflineConsumer);
    }

    /**
     * Removes consumer for any changes in the status of the offline availability of the given
     * story.
     */
    public void removeOfflineStatusConsumer(String url, Consumer<Boolean> isOfflineConsumer) {
        if (!mOfflineStatusConsumersMap.containsKey(url)) {
            Logger.w(TAG, "Removing consumer for url %s with no list of consumers", url);
            return;
        }

        if (!mOfflineStatusConsumersMap.get(url).remove(isOfflineConsumer)) {
            Logger.w(TAG, "Removing consumer for url %s that isn't on list of consumers", url);
        }

        if (mOfflineStatusConsumersMap.get(url).isEmpty()) {
            mOfflineStatusConsumersMap.remove(url);
        }
    }

    private void notifyConsumers(String url, boolean availableOffline) {
        List<Consumer<Boolean>> offlineStatusConsumers = mOfflineStatusConsumersMap.get(url);
        if (offlineStatusConsumers == null) {
            return;
        }

        for (Consumer<Boolean> statusConsumer : offlineStatusConsumers) {
            statusConsumer.accept(availableOffline);
        }
    }

    public void requestOfflineStatusForNewContent() {
        if (mContentToRequestStatus.isEmpty()) {
            return;
        }

        mOfflineIndicatorApi.getOfflineStatus(
                new ArrayList<>(mContentToRequestStatus), offlineUrls -> {
                    for (String offlineUrl : offlineUrls) {
                        updateOfflineStatus(offlineUrl, true);
                    }
                });
        mContentToRequestStatus.clear();
    }

    @Override
    public void updateOfflineStatus(String url, boolean availableOffline) {
        // If the new offline status is the same as our knowledge of it, no-op.
        if (mOfflineContent.contains(url) == availableOffline) {
            return;
        }

        if (availableOffline) {
            mOfflineContent.add(url);
        } else {
            mOfflineContent.remove(url);
        }

        notifyConsumers(url, availableOffline);
    }

    public void onDestroy() {
        mOfflineStatusConsumersMap.clear();
        mOfflineIndicatorApi.removeOfflineStatusListener(this);
    }
}
