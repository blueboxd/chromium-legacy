// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.feed.action.FeedActionHandler;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;

/** Implementation of the {@link ActionApi} for the explore surface. */
class ExploreSurfaceActionHandler extends FeedActionHandler {
    ExploreSurfaceActionHandler(@NonNull NativePageNavigationDelegate delegate,
            @NonNull Runnable suggestionConsumedObserver,
            @NonNull FeedOfflineIndicator offlineIndicator,
            @NonNull OfflinePageBridge offlinePageBridge,
            @NonNull FeedLoggingBridge loggingBridge,
            Activity activity, Profile profile) {
        super(delegate, suggestionConsumedObserver, offlineIndicator, offlinePageBridge,
                loggingBridge, activity, profile);
    }

    // TODO(crbug.com/982018): Support download and lean more actions.

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {}

    @Override
    public boolean canDownloadUrl() {
        return false;
    }

    @Override
    public boolean canOpenUrlInIncognitoMode() {
        return false;
    }

    @Override
    public boolean canOpenUrlInNewTab() {
        return false;
    }

    @Override
    public void learnMore() {}

    @Override
    public boolean canLearnMore() {
        return false;
    }
}
