// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.SyncContentResolverDelegate;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

/**
 * A utility class for mocking AndroidSyncSettings.
 */
public class AndroidSyncSettingsTestUtils {
    /**
     * Sets up AndroidSyncSettings with a mock SyncContentResolverDelegate and with Android's master
     * sync setting enabled. (This setting is found, e.g., under Settings > Accounts & Sync >
     * Auto-sync data.)
     */
    @CalledByNative
    @VisibleForTesting
    public static void setUpAndroidSyncSettingsForTesting() {
        setUpAndroidSyncSettingsForTesting(new MockSyncContentResolverDelegate());
    }

    @VisibleForTesting
    public static void setUpAndroidSyncSettingsForTesting(SyncContentResolverDelegate delegate) {
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(delegate, null);
    }
}
