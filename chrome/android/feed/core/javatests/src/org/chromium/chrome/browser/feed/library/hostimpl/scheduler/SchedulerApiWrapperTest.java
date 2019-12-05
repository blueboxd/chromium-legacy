// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.scheduler;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi.SessionState;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link SchedulerApiWrapper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SchedulerApiWrapperTest {
    @Mock
    private ThreadUtils mThreadUtils;
    @Mock
    private SchedulerApi mSchedulerApi;
    private SessionState mSessionState;

    private final FakeMainThreadRunner mMainThreadRunner =
            FakeMainThreadRunner.runTasksImmediately();

    @Before
    public void setup() {
        mSessionState = new SessionState(false, 0L, false);
        initMocks(this);
    }

    @After
    public void validate() {
        validateMockitoUsage();
    }

    @Test
    public void testShouldSessionRequestData_mainThread() {
        when(mThreadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.shouldSessionRequestData(mSessionState);
        verify(mSchedulerApi).shouldSessionRequestData(mSessionState);
        assertThat(mMainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testShouldSessionRequestData_backgroundThread() {
        when(mThreadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.shouldSessionRequestData(mSessionState);
        verify(mSchedulerApi).shouldSessionRequestData(mSessionState);
        assertThat(mMainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }

    @Test
    public void testOnReceiveNewContent_mainThread() {
        when(mThreadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.onReceiveNewContent(0);
        verify(mSchedulerApi).onReceiveNewContent(0);
        assertThat(mMainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testOnReceiveNewContent_backgroundThread() {
        when(mThreadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.onReceiveNewContent(0);
        verify(mSchedulerApi).onReceiveNewContent(0);
        assertThat(mMainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }

    @Test
    public void testOnRequestError_mainThread() {
        when(mThreadUtils.isMainThread()).thenReturn(true);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.onRequestError(0);
        verify(mSchedulerApi).onRequestError(0);
        assertThat(mMainThreadRunner.hasTasks()).isFalse();
    }

    @Test
    public void testOnRequestError_backgroundThread() {
        when(mThreadUtils.isMainThread()).thenReturn(false);
        SchedulerApiWrapper wrapper =
                new SchedulerApiWrapper(mSchedulerApi, mThreadUtils, mMainThreadRunner);
        wrapper.onRequestError(0);
        verify(mSchedulerApi).onRequestError(0);
        assertThat(mMainThreadRunner.getCompletedTaskCount()).isEqualTo(1);
    }
}
