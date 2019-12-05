// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link FeedModelProviderFactory}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedModelProviderFactoryTest {
    private final FakeBasicLoggingApi mFakeBasicLoggingApi = new FakeBasicLoggingApi();
    @Mock
    private ThreadUtils mThreadUtils;
    @Mock
    private TimingUtils mTimingUtils;
    @Mock
    private FeedSessionManager mFeedSessionManager;
    @Mock
    private Configuration mConfig;
    @Mock
    private TaskQueue mTaskQueue;

    private MainThreadRunner mMainThreadRunner;

    @Before
    public void setUp() {
        initMocks(this);

        mMainThreadRunner = FakeMainThreadRunner.queueAllTasks();

        when(mConfig.getValueOrDefault(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, 0L)).thenReturn(0L);
        when(mConfig.getValueOrDefault(ConfigKey.NON_CACHED_PAGE_SIZE, 0L)).thenReturn(0L);
        when(mConfig.getValueOrDefault(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, 0L)).thenReturn(0L);
    }

    @Test
    public void testModelProviderFactory() {
        FeedModelProviderFactory factory =
                new FeedModelProviderFactory(mFeedSessionManager, mThreadUtils, mTimingUtils,
                        mTaskQueue, mMainThreadRunner, mConfig, mFakeBasicLoggingApi);
        assertThat(factory.createNew(null, UiContext.getDefaultInstance())).isNotNull();
    }
}
