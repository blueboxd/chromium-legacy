// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.SessionEvent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError.ErrorType;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelFeature;
import com.google.android.libraries.feed.testing.modelprovider.FakeModelProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link UiSessionRequestLogger}. */
@RunWith(RobolectricTestRunner.class)
public class UiSessionRequestLoggerTest {
    private static final long SESSION_EVENT_DELAY = 123L;
    private static final ModelError MODEL_ERROR =
            new ModelError(ErrorType.UNKNOWN, /* continuationToken= */ null);
    private static final FakeModelFeature MODEL_FEATURE = FakeModelFeature.newBuilder().build();

    @Mock
    private BasicLoggingApi basicLoggingApi;

    private FakeClock clock;
    private UiSessionRequestLogger uiSessionRequestLogger;
    private FakeModelProvider modelProvider;

    @Before
    public void setUp() {
        initMocks(this);
        clock = new FakeClock();
        modelProvider = new FakeModelProvider();
        uiSessionRequestLogger = new UiSessionRequestLogger(clock, basicLoggingApi);
    }

    @Test
    public void testOnSessionRequested_onSessionStart_logsSessionStart() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);
        clock.advance(SESSION_EVENT_DELAY);

        modelProvider.triggerOnSessionStart(MODEL_FEATURE);

        verify(basicLoggingApi)
                .onInitialSessionEvent(
                        SessionEvent.STARTED, (int) SESSION_EVENT_DELAY, /* sessionCount= */ 1);
        assertThat(modelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onSessionFinished_logsSessionStart() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);
        clock.advance(SESSION_EVENT_DELAY);

        modelProvider.triggerOnSessionFinished();

        verify(basicLoggingApi)
                .onInitialSessionEvent(SessionEvent.FINISHED_IMMEDIATELY, (int) SESSION_EVENT_DELAY,
                        /* sessionCount= */ 1);
        assertThat(modelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onSessionError_logsSessionStart() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);
        clock.advance(SESSION_EVENT_DELAY);

        modelProvider.triggerOnError(MODEL_ERROR);

        verify(basicLoggingApi)
                .onInitialSessionEvent(
                        SessionEvent.ERROR, (int) SESSION_EVENT_DELAY, /* sessionCount= */ 1);
        assertThat(modelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testOnSessionRequested_onDestroy_logsDestroy() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);
        clock.advance(SESSION_EVENT_DELAY);

        uiSessionRequestLogger.onDestroy();

        verify(basicLoggingApi)
                .onInitialSessionEvent(SessionEvent.USER_ABANDONED, (int) SESSION_EVENT_DELAY,
                        /* sessionCount= */ 1);
        assertThat(modelProvider.getObservers()).isEmpty();
    }

    @Test
    public void testMultipleSessions_incrementsSessionCount() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);
        modelProvider.triggerOnSessionStart(MODEL_FEATURE);

        uiSessionRequestLogger.onSessionRequested(modelProvider);
        modelProvider.triggerOnSessionFinished();

        uiSessionRequestLogger.onSessionRequested(modelProvider);
        modelProvider.triggerOnError(MODEL_ERROR);

        InOrder inOrder = inOrder(basicLoggingApi);

        inOrder.verify(basicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.STARTED), anyInt(), eq(1));
        inOrder.verify(basicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.FINISHED_IMMEDIATELY), anyInt(), eq(2));
        inOrder.verify(basicLoggingApi)
                .onInitialSessionEvent(eq(SessionEvent.ERROR), anyInt(), eq(3));
    }

    @Test
    public void testOnSessionRequested_immediatelyTriggerSessionStart_logsSessionStart() {
        modelProvider.triggerOnSessionStartImmediately(MODEL_FEATURE);

        uiSessionRequestLogger.onSessionRequested(modelProvider);

        // The ModelProvider will sometimes trigger onSessionStart() immediately when a listener is
        // registered. This tests that that situation is appropriately logged.
        verify(basicLoggingApi).onInitialSessionEvent(SessionEvent.STARTED, 0, 1);
    }

    @Test
    public void testOnSessionRequested_twice_deregistersFromFirstModelProvider() {
        uiSessionRequestLogger.onSessionRequested(modelProvider);

        uiSessionRequestLogger.onSessionRequested(new FakeModelProvider());

        assertThat(modelProvider.getObservers()).isEmpty();
    }
}
