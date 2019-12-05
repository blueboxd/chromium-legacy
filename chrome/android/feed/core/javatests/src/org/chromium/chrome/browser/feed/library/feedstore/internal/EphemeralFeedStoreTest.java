// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.feedstore.internal;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.feedstore.testing.AbstractClearableFeedStoreTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link EphemeralFeedStore} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EphemeralFeedStoreTest extends AbstractClearableFeedStoreTest {
    @Before
    public void setUp() throws Exception {
        initMocks(this);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        return new EphemeralFeedStore(mFakeClock, mTimingUtils, new FeedStoreHelper());
    }
}
