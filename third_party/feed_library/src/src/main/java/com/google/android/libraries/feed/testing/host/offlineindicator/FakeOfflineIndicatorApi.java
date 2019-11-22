// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.host.offlineindicator;

import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.common.functional.Consumer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Fake used for tests using the {@link OfflineIndicatorApi}. */
public class FakeOfflineIndicatorApi implements OfflineIndicatorApi {
    private final Set<String> offlineUrls;
    private final Set<OfflineStatusListener> listeners;

    private FakeOfflineIndicatorApi(String[] urls) {
        offlineUrls = new HashSet<>(Arrays.asList(urls));
        listeners = new HashSet<>();
    }

    public static FakeOfflineIndicatorApi createWithOfflineUrls(String... urls) {
        return new FakeOfflineIndicatorApi(urls);
    }

    public static FakeOfflineIndicatorApi createWithNoOfflineUrls() {
        return createWithOfflineUrls();
    }

    @Override
    public void getOfflineStatus(
            List<String> urlsToRetrieve, Consumer<List<String>> urlListConsumer) {
        HashSet<String> copiedHashSet = new HashSet<>(offlineUrls);
        copiedHashSet.retainAll(urlsToRetrieve);

        urlListConsumer.accept(new ArrayList<>(copiedHashSet));
    }

    @Override
    public void addOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        listeners.add(offlineStatusListener);
    }

    @Override
    public void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        listeners.remove(offlineStatusListener);
    }

    /**
     * Sets the offline status of the given {@code url} to the given value and notifies any
     * observers.
     */
    public void setOfflineStatus(String url, boolean isAvailable) {
        boolean statusChanged = isAvailable ? offlineUrls.add(url) : offlineUrls.remove(url);

        if (!statusChanged) {
            return;
        }

        for (OfflineStatusListener listener : listeners) {
            listener.updateOfflineStatus(url, isAvailable);
        }
    }
}
