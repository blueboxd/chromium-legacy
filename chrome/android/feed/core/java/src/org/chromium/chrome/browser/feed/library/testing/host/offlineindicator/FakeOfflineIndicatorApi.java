// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.host.offlineindicator;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.offlineindicator.OfflineIndicatorApi;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Fake used for tests using the {@link OfflineIndicatorApi}. */
public class FakeOfflineIndicatorApi implements OfflineIndicatorApi {
    private final Set<String> mOfflineUrls;
    private final Set<OfflineStatusListener> mListeners;

    private FakeOfflineIndicatorApi(String[] urls) {
        mOfflineUrls = new HashSet<>(Arrays.asList(urls));
        mListeners = new HashSet<>();
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
        HashSet<String> copiedHashSet = new HashSet<>(mOfflineUrls);
        copiedHashSet.retainAll(urlsToRetrieve);

        urlListConsumer.accept(new ArrayList<>(copiedHashSet));
    }

    @Override
    public void addOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        mListeners.add(offlineStatusListener);
    }

    @Override
    public void removeOfflineStatusListener(OfflineStatusListener offlineStatusListener) {
        mListeners.remove(offlineStatusListener);
    }

    /**
     * Sets the offline status of the given {@code url} to the given value and notifies any
     * observers.
     */
    public void setOfflineStatus(String url, boolean isAvailable) {
        boolean statusChanged = isAvailable ? mOfflineUrls.add(url) : mOfflineUrls.remove(url);

        if (!statusChanged) {
            return;
        }

        for (OfflineStatusListener listener : mListeners) {
            listener.updateOfflineStatus(url, isAvailable);
        }
    }
}
