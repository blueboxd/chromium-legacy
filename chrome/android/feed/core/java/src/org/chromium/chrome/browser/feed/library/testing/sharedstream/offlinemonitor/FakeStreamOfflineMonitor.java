// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.sharedstream.offlinemonitor;

import org.chromium.chrome.browser.feed.library.sharedstream.offlinemonitor.StreamOfflineMonitor;
import org.chromium.chrome.browser.feed.library.testing.host.offlineindicator.FakeOfflineIndicatorApi;

/** Fake used for tests using the {@link StreamOfflineMonitor}. */
public class FakeStreamOfflineMonitor extends StreamOfflineMonitor {
    private final FakeOfflineIndicatorApi mFakeIndicatorApi;
    private boolean mOfflineStatusRequested;

    /**
     * Creates a {@link FakeStreamOfflineMonitor} with the given {@link FakeOfflineIndicatorApi}.
     */
    public static FakeStreamOfflineMonitor createWithOfflineIndicatorApi(
            FakeOfflineIndicatorApi offlineIndicatorApi) {
        return new FakeStreamOfflineMonitor(offlineIndicatorApi);
    }

    /**
     * Creates a {@link FakeStreamOfflineMonitor} with a default {@link FakeOfflineIndicatorApi}.
     */
    public static FakeStreamOfflineMonitor create() {
        return new FakeStreamOfflineMonitor(FakeOfflineIndicatorApi.createWithNoOfflineUrls());
    }

    private FakeStreamOfflineMonitor(FakeOfflineIndicatorApi offlineIndicatorApi) {
        super(offlineIndicatorApi);
        this.mFakeIndicatorApi = offlineIndicatorApi;
    }

    /** Sets the offline status for the given {@code url}. */
    public void setOfflineStatus(String url, boolean isAvailable) {
        // Check if the url is available to marks the url as something to request from the
        // OfflineIndicatorApi.
        isAvailableOffline(url);

        // Sets the status of the url with the api, which is the source of truth.
        mFakeIndicatorApi.setOfflineStatus(url, isAvailable);

        // Triggers notification to any current listeners, namely the superclass of this fake.
        requestOfflineStatusForNewContent();
    }

    /** Returns the count of how many observers there are for offline status. */
    public int getOfflineStatusConsumerCount() {
        return mOfflineStatusConsumersMap.size();
    }

    @Override
    public void requestOfflineStatusForNewContent() {
        mOfflineStatusRequested = true;
        super.requestOfflineStatusForNewContent();
    }

    public boolean isOfflineStatusRequested() {
        return mOfflineStatusRequested;
    }
}
