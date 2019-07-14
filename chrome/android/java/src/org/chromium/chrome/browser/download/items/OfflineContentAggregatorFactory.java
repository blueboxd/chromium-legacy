// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.OfflineContentProvider;

/**
 * Basic factory that creates and returns an {@link OfflineContentProvider} that is attached
 * natively to {@link Profile}.
 */
public class OfflineContentAggregatorFactory {
    // TODO(crbug.com/857543): Remove this after downloads have implemented it.
    // We need only one provider, since OfflineContentAggregator lives in the original profile.
    private static OfflineContentProvider sProvider;

    private OfflineContentAggregatorFactory() {}

    /**
     * Allows tests to push a custom {@link OfflineContentProvider} to be used instead of the one
     * pulled from a {@link Profile}.
     * @param provider The {@link OfflineContentProvider} to return.  If {@code null}, will revert
     *                 to the non-overriding behavior and pull a {link OfflineContentProvider} from
     *                 {@link Profile}.
     */
    @VisibleForTesting
    public static void setOfflineContentProviderForTests(
            @Nullable OfflineContentProvider provider) {
        if (provider == null) {
            sProvider = null;
        } else {
            sProvider = getProvider(provider);
        }
    }

    /**
     * Used to get access to the offline content aggregator.
     * @return An {@link OfflineContentProvider} instance representing the offline content
     *         aggregator.
     */
    public static OfflineContentProvider get() {
        if (sProvider == null) {
            sProvider = getProvider(nativeGetOfflineContentAggregator());
        }
        return sProvider;
    }

    private static OfflineContentProvider getProvider(OfflineContentProvider provider) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return provider;
        } else {
            return new DownloadBlockedOfflineContentProvider(provider);
        }
    }

    private static native OfflineContentProvider nativeGetOfflineContentAggregator();
}
