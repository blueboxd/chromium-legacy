// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.logging;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.common.time.testing.FakeClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link SpinnerLogger}. */
@RunWith(RobolectricTestRunner.class)
public class SpinnerLoggerTest {
    private static final long START_TIME = 1000;
    private static final int SPINNER_TIME = 500;

    @Mock
    private BasicLoggingApi basicLoggingApi;
    private FakeClock clock;
    private SpinnerLogger spinnerLogger;

    @Before
    public void setUp() {
        initMocks(this);
        clock = new FakeClock();
        clock.set(START_TIME);

        spinnerLogger = new SpinnerLogger(basicLoggingApi, clock);
    }

    @Test
    public void spinnerStarted_logsSpinnerStarted() {
        spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        verify(basicLoggingApi).onSpinnerStarted(SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void spinnerStarted_twice_throwsException() {
        spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        assertThatRunnable(() -> spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void spinnerFinished_logsOnSpinnerFinished_ifSpinnerStartedCalledBefore() {
        spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);
        clock.advance(SPINNER_TIME);

        spinnerLogger.spinnerFinished();

        verify(basicLoggingApi).onSpinnerFinished(SPINNER_TIME, SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void spinnerFinished_beforeStarting_throwsException() {
        assertThatRunnable(() -> spinnerLogger.spinnerFinished())
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void spinnerDestroyedWithoutCompleting_logsOnSpinnerDestroyedWithoutCompleting() {
        spinnerLogger.spinnerStarted(SpinnerType.MORE_BUTTON);
        clock.advance(SPINNER_TIME);

        spinnerLogger.spinnerDestroyedWithoutCompleting();

        verify(basicLoggingApi)
                .onSpinnerDestroyedWithoutCompleting(SPINNER_TIME, SpinnerType.MORE_BUTTON);
    }

    @Test
    public void spinnerDestroyed_beforeStarting_throwsException() {
        assertThatRunnable(() -> spinnerLogger.spinnerDestroyedWithoutCompleting())
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void isSpinnerActive_returnsTrue_ifSpinnerStartedCalledBefore() {
        spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        assertThat(spinnerLogger.isSpinnerActive()).isTrue();
    }

    @Test
    public void isSpinnerActive_returnsFalseInitially() {
        assertThat(spinnerLogger.isSpinnerActive()).isFalse();
    }

    @Test
    public void isSpinnerActive_returnsFalse_ifSpinnerFinishedCalledBefore() {
        spinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);
        spinnerLogger.spinnerFinished();

        assertThat(spinnerLogger.isSpinnerActive()).isFalse();
    }
}
