// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.scheduler;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi.SessionState;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link SchedulerApiWrapper}. */
@RunWith(RobolectricTestRunner.class)
public class SchedulerApiWrapperTest {
    @Mock
    private ThreadUtils threadUtils;
    @Mock
    private SchedulerApi schedulerApi;
    private SessionState sessionState;

    private final FakeMainThreadRunner mainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    @Before
    public void setup() {
        sessionState = new SessionState(false, 0L, false);
        initMocks(this);
    }

    @After
    public void validate() {
        validateMockitoUsage();
    }

    @Test
    public void testShouldSessionRequestData_mainThread() {
        when(threadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.shouldSessionRequestData(sessionState);
        verify(schedulerApi).shouldSessionRequestData(sessionState);
        assertThat(mainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testShouldSessionRequestData_backgroundThread() {
        when(threadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.shouldSessionRequestData(sessionState);
        verify(schedulerApi).shouldSessionRequestData(sessionState);
        assertThat(mainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }

    @Test
    public void testOnReceiveNewContent_mainThread() {
        when(threadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.onReceiveNewContent(0);
        verify(schedulerApi).onReceiveNewContent(0);
        assertThat(mainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testOnReceiveNewContent_backgroundThread() {
        when(threadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.onReceiveNewContent(0);
        verify(schedulerApi).onReceiveNewContent(0);
        assertThat(mainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }

    @Test
    public void testOnRequestError_mainThread() {
        when(threadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.onRequestError(0);
        verify(schedulerApi).onRequestError(0);
        assertThat(mainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testOnRequestError_backgroundThread() {
        when(threadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(schedulerApi, threadUtils, mainThreadRunner);
        wrapper.onRequestError(0);
        verify(schedulerApi).onRequestError(0);
        assertThat(mainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }
}
