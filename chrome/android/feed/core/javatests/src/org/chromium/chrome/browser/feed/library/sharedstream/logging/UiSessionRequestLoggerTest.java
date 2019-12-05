// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.SessionEvent;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError.ErrorType;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelFeature;
import org.chromium.chrome.browser.feed.library.testing.modelprovider.FakeModelProvider;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link UiSessionRequestLogger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UiSessionRequestLoggerTest {
    private static final long SESSION_EVENT_DELAY = 123L;
    private static final ModelError MODEL_ERROR =
            new ModelError(ErrorType.UNKNOWN, /* continuationToken= */ null);
    private static final FakeModelFeature MODEL_FEATURE = FakeModelFeature.newBuilder().build();

    @Mock
    private BasicLoggingApi mBasicLoggingApi;

    private FakeClock mClock;
    private UiSessionRequestLogger mUiSessionRequestLogger;
    private FakeModelProvider mModelProvider;

    @Before
    public void setUp() {
        initMocks(this);
        mClock = new FakeClock();
        mModelProvider = new FakeModelProvider();
        mUiSessionRequestLogger = new UiSessionRequestLogger(mClock, mBasicLoggingApi);
    }

    @Test
    public void testOnSessionRequested_onSessionStart_logsSessionStart() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mClock.advance(SESSION_EVENT_DELAY);

        mModelProvider.triggerOnSessionStart(MODEL_FEATURE);

        verify(mBasicLoggingApi)
                .onInitialSessionEvent(
                        SessionEvent.STARTED, (int) SESSION_EVENT_DELAY, /* sessionCount= */ 1);
        assertThat(mModelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onSessionFinished_logsSessionStart() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mClock.advance(SESSION_EVENT_DELAY);

        mModelProvider.triggerOnSessionFinished();

        verify(mBasicLoggingApi)
                .onInitialSessionEvent(SessionEvent.FINISHED_IMMEDIATELY, (int) SESSION_EVENT_DELAY,
                        /* sessionCount= */ 1);
        assertThat(mModelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onSessionError_logsSessionStart() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mClock.advance(SESSION_EVENT_DELAY);

        mModelProvider.triggerOnError(MODEL_ERROR);

        verify(mBasicLoggingApi)
                .onInitialSessionEvent(
                        SessionEvent.ERROR, (int) SESSION_EVENT_DELAY, /* sessionCount= */ 1);
        assertThat(mModelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onDestroy_logsDestroy() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mClock.advance(SESSION_EVENT_DELAY);

        mUiSessionRequestLogger.onDestroy();

        verify(mBasicLoggingApi)
                .onInitialSessionEvent(SessionEvent.USER_ABANDONED, (int) SESSION_EVENT_DELAY,
                        /* sessionCount= */ 1);
        assertThat(mModelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testMultipleSessions_incrementsSessionCount() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mModelProvider.triggerOnSessionStart(MODEL_FEATURE);

        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mModelProvider.triggerOnSessionFinished();

        mUiSessionRequestLogger.onSessionRequested(mModelProvider);
        mModelProvider.triggerOnError(MODEL_ERROR);

        InOrder inOrder = inOrder(mBasicLoggingApi);

        inOrder.verify(mBasicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.STARTED), anyInt(), eq(1));
        inOrder.verify(mBasicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.FINISHED_IMMEDIATELY), anyInt(), eq(2));
        inOrder.verify(mBasicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.ERROR), anyInt(), eq(3));
    }

    @Test
    public void testOnSessionRequested_immediatelyTriggerSessionStart_logsSessionStart() {
        mModelProvider.triggerOnSessionStartImmediately(MODEL_FEATURE);

        mUiSessionRequestLogger.onSessionRequested(mModelProvider);

        // The ModelProvider will sometimes trigger onSessionStart() immediately when a listener is
        // registered. This tests that that situation is appropriately logged.
        verify(mBasicLoggingApi).onInitialSessionEvent(SessionEvent.STARTED, 0, 1);
    }

    @Test
    public void testOnSessionRequested_twice_deregistersFromFirstModelProvider() {
        mUiSessionRequestLogger.onSessionRequested(mModelProvider);

        mUiSessionRequestLogger.onSessionRequested(new FakeModelProvider());

        assertThat(mModelProvider.getObservers()).isEmpty();
    }
}
