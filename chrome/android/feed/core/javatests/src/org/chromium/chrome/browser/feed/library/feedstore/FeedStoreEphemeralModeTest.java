// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedstore;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.feedstore.testing.AbstractFeedStoreTest;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryContentStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryJournalStorage;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/** Tests of the {@link FeedStore} class when running in ephemeral mode */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedStoreEphemeralModeTest extends AbstractFeedStoreTest {
    private final FakeBasicLoggingApi mFakeBasicLoggingApi = new FakeBasicLoggingApi();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry mExtensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);
    private final ContentStorageDirect mContentStorage = new InMemoryContentStorage();

    @Mock
    private Configuration mConfiguration;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        when(mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false))
                .thenReturn(false);
    }

    @Override
    protected Store getStore(MainThreadRunner mainThreadRunner) {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        FeedStore feedStore = new FeedStore(mConfiguration, mTimingUtils, mExtensionRegistry,
                mContentStorage, new InMemoryJournalStorage(), mFakeThreadUtils, fakeTaskQueue,
                mFakeClock, mFakeBasicLoggingApi, mainThreadRunner);
        mFakeThreadUtils.enforceMainThread(false);
        feedStore.switchToEphemeralMode();
        return feedStore;
    }
}
