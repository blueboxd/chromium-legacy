// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.scope;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.internal.lifecycle.Resettable;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedAppLifecycleListener;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ClearAllListener} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClearAllListenerTest {
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Mock
    private Resettable mStore;
    @Mock
    private FeedSessionManager mSessionManager;
    private FakeTaskQueue mFakeTaskQueue;
    private FeedAppLifecycleListener mFeedAppLifecycleListener;

    @Before
    public void setUp() {
        initMocks(this);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeTaskQueue.initialize(() -> {});
        mFeedAppLifecycleListener = new FeedAppLifecycleListener(mFakeThreadUtils);
    }

    @Test
    public void testClearAll() {
        setupClearAllListener();
        mFeedAppLifecycleListener.onClearAll();
        verify(mSessionManager).reset();
        verify(mStore).reset();
        assertThat(mFakeTaskQueue.resetWasCalled()).isTrue();
        assertThat(mFakeTaskQueue.completeResetWasCalled()).isTrue();
    }

    @Test
    public void testClearAllWithRefresh() {
        setupClearAllListener();
        mFeedAppLifecycleListener.onClearAllWithRefresh();
        verify(mSessionManager).reset();
        verify(mSessionManager)
                .triggerRefresh(null, RequestReason.CLEAR_ALL, UiContext.getDefaultInstance());
        verify(mStore).reset();
        assertThat(mFakeTaskQueue.resetWasCalled()).isTrue();
        assertThat(mFakeTaskQueue.completeResetWasCalled()).isTrue();
    }

    @Test
    public void testNonClearLifecycld() {
        setupClearAllListener();
        mFeedAppLifecycleListener.onEnterForeground();
        verifyZeroInteractions(mSessionManager);
        verifyZeroInteractions(mStore);
    }

    private void setupClearAllListener() {
        new ClearAllListener(mFakeTaskQueue, mSessionManager, mStore, mFakeThreadUtils,
                mFeedAppLifecycleListener);
    }
}
