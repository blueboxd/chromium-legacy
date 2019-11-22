// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.offlinemonitor;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi.OfflineStatusListener;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Monitors and notifies consumers as to when the offline status of given urls change. */
public class StreamOfflineMonitor implements OfflineStatusListener {
    private static final String TAG = "StreamOfflineMonitor";

    private final Set<String> contentToRequestStatus = new HashSet<>();
    private final Set<String> offlineContent = new HashSet<>();
    private final OfflineIndicatorApi offlineIndicatorApi;

    @VisibleForTesting
    protected final Map<String, List<Consumer<Boolean>>> offlineStatusConsumersMap =
            new HashMap<>();

    @SuppressWarnings("nullness:argument.type.incompatible")
    public StreamOfflineMonitor(OfflineIndicatorApi offlineIndicatorApi) {
        offlineIndicatorApi.addOfflineStatusListener(this);
        this.offlineIndicatorApi = offlineIndicatorApi;
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
        if (offlineContent.contains(url)) {
            return true;
        }
        contentToRequestStatus.add(url);
        return false;
    }

    /**
     * Adds consumer for any changes in the status of the offline availability of the given story.
     * Only triggered on change, not immediately upon registering.
     */
    public void addOfflineStatusConsumer(String url, Consumer<Boolean> isOfflineConsumer) {
        if (!offlineStatusConsumersMap.containsKey(url)) {
            // Initializing size of lists to 1 as it is unlikely that we would have more than one
            // listener per URL.
            offlineStatusConsumersMap.put(url, new ArrayList<>(1));
        }

        offlineStatusConsumersMap.get(url).add(isOfflineConsumer);
    }

    /**
     * Removes consumer for any changes in the status of the offline availability of the given
     * story.
     */
    public void removeOfflineStatusConsumer(String url, Consumer<Boolean> isOfflineConsumer) {
        if (!offlineStatusConsumersMap.containsKey(url)) {
            Logger.w(TAG, "Removing consumer for url %s with no list of consumers", url);
            return;
        }

        if (!offlineStatusConsumersMap.get(url).remove(isOfflineConsumer)) {
            Logger.w(TAG, "Removing consumer for url %s that isn't on list of consumers", url);
        }

        if (offlineStatusConsumersMap.get(url).isEmpty()) {
            offlineStatusConsumersMap.remove(url);
        }
    }

    private void notifyConsumers(String url, boolean availableOffline) {
        List<Consumer<Boolean>> offlineStatusConsumers = offlineStatusConsumersMap.get(url);
        if (offlineStatusConsumers == null) {
            return;
        }

        for (Consumer<Boolean> statusConsumer : offlineStatusConsumers) {
            statusConsumer.accept(availableOffline);
        }
    }

    public void requestOfflineStatusForNewContent() {
        if (contentToRequestStatus.isEmpty()) {
            return;
        }

        offlineIndicatorApi.getOfflineStatus(
                new ArrayList<>(contentToRequestStatus), offlineUrls -> {
                    for (String offlineUrl : offlineUrls) {
                        updateOfflineStatus(offlineUrl, true);
                    }
                });
        contentToRequestStatus.clear();
    }

    @Override
    public void updateOfflineStatus(String url, boolean availableOffline) {
        // If the new offline status is the same as our knowledge of it, no-op.
        if (offlineContent.contains(url) == availableOffline) {
            return;
        }

        if (availableOffline) {
            offlineContent.add(url);
        } else {
            offlineContent.remove(url);
        }

        notifyConsumers(url, availableOffline);
    }

    public void onDestroy() {
        offlineStatusConsumersMap.clear();
        offlineIndicatorApi.removeOfflineStatusListener(this);
    }
}
