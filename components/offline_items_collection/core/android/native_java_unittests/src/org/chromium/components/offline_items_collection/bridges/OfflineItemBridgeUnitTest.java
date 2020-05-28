// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection.bridges;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * Unit test to verify {@link OfflineItemBridge} can create {@link OfflineItem} correctly through
 * JNI bridge from native.
 */
public class OfflineItemBridgeUnitTest {
    @CalledByNative
    private OfflineItemBridgeUnitTest() {}

    @CalledByNative
    public void testCreateDefaultOfflineItem(OfflineItem item) {
        // TODO(xingliu): Verify OfflineItemSchedule object and other fields.
        Assert.assertNotNull(item);
    }
}