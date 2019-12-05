// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link SpinnerLogger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SpinnerLoggerTest {
    private static final long START_TIME = 1000;
    private static final int SPINNER_TIME = 500;

    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    private FakeClock mClock;
    private SpinnerLogger mSpinnerLogger;

    @Before
    public void setUp() {
        initMocks(this);
        mClock = new FakeClock();
        mClock.set(START_TIME);

        mSpinnerLogger = new SpinnerLogger(mBasicLoggingApi, mClock);
    }

    @Test
    public void spinnerStarted_logsSpinnerStarted() {
        mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        verify(mBasicLoggingApi).onSpinnerStarted(SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void spinnerStarted_twice_throwsException() {
        mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        assertThatRunnable(() -> mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD))
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void spinnerFinished_logsOnSpinnerFinished_ifSpinnerStartedCalledBefore() {
        mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);
        mClock.advance(SPINNER_TIME);

        mSpinnerLogger.spinnerFinished();

        verify(mBasicLoggingApi).onSpinnerFinished(SPINNER_TIME, SpinnerType.INITIAL_LOAD);
    }

    @Test
    public void spinnerFinished_beforeStarting_throwsException() {
        assertThatRunnable(() -> mSpinnerLogger.spinnerFinished())
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void spinnerDestroyedWithoutCompleting_logsOnSpinnerDestroyedWithoutCompleting() {
        mSpinnerLogger.spinnerStarted(SpinnerType.MORE_BUTTON);
        mClock.advance(SPINNER_TIME);

        mSpinnerLogger.spinnerDestroyedWithoutCompleting();

        verify(mBasicLoggingApi)
                .onSpinnerDestroyedWithoutCompleting(SPINNER_TIME, SpinnerType.MORE_BUTTON);
    }

    @Test
    public void spinnerDestroyed_beforeStarting_throwsException() {
        assertThatRunnable(() -> mSpinnerLogger.spinnerDestroyedWithoutCompleting())
                .throwsAnExceptionOfType(RuntimeException.class);
    }

    @Test
    public void isSpinnerActive_returnsTrue_ifSpinnerStartedCalledBefore() {
        mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);

        assertThat(mSpinnerLogger.isSpinnerActive()).isTrue();
    }

    @Test
    public void isSpinnerActive_returnsFalseInitially() {
        assertThat(mSpinnerLogger.isSpinnerActive()).isFalse();
    }

    @Test
    public void isSpinnerActive_returnsFalse_ifSpinnerFinishedCalledBefore() {
        mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);
        mSpinnerLogger.spinnerFinished();

        assertThat(mSpinnerLogger.isSpinnerActive()).isFalse();
    }
}
