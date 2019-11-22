// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.feedstore.internal;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.feedstore.testing.AbstractClearableFeedStoreTest;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link EphemeralFeedStore} class. */
@RunWith(RobolectricTestRunner.class)
public class EphemeralFeedStoreTest extends AbstractClearableFeedStoreTest {
    @Before
    public void setUp() throws Exception {
        initMocks(this);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new EphemeralFeedStore(fakeClock, timingUtils, new FeedStoreHelper());
    }
}
