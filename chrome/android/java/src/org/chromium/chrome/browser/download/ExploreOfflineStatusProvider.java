// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/**
 * Keeps track of prefetch content addition and removal from chrome. This information is used to
 * decide whether or not users should be see a message on NTP about offline content availability in
 * chrome.
 */
public class ExploreOfflineStatusProvider implements OfflineContentProvider.Observer {
    private static ExploreOfflineStatusProvider sInstance;

    private Set<ContentId> mPrefetchItems = new HashSet<>();

    /**
     * @return An {@link ExploreOfflineStatusProvider} instance singleton.  If one
     *         is not available this will create a new one.
     */
    public static ExploreOfflineStatusProvider getInstance() {
        if (sInstance == null) {
            sInstance = new ExploreOfflineStatusProvider();
        }
        return sInstance;
    }

    private ExploreOfflineStatusProvider() {
        OfflineContentProvider provider = OfflineContentAggregatorFactory.get();
        provider.addObserver(this);
        provider.getAllItems(this::onItemsAdded);
    }

    /**
     * @return Whether or not there are any prefetch content available in chrome.
     */
    public boolean isPrefetchContentAvailable() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS, false);
    }

    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        if (items.isEmpty()) return;

        for (OfflineItem item : items) {
            if (item.isSuggested) mPrefetchItems.add(item.id);
        }
        updateSharedPref();
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (mPrefetchItems.isEmpty()) return;

        mPrefetchItems.remove(id);
        updateSharedPref();
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {}

    private void updateSharedPref() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                !mPrefetchItems.isEmpty());
    }
}
