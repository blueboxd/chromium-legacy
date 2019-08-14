// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.job.JobParameters;
import android.os.PersistableBundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link BackgroundTaskJobService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskJobServiceTest {
    private static BackgroundTaskSchedulerJobService.Clock sClock = () -> 1415926535000L;
    private static BackgroundTaskSchedulerJobService.Clock sZeroClock = () -> 0L;
    @Mock
    private BackgroundTaskSchedulerDelegate mDelegate;
    @Mock
    private BackgroundTaskSchedulerUma mBackgroundTaskSchedulerUma;
    @Mock
    private BackgroundTaskSchedulerImpl mBackgroundTaskSchedulerImpl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(
                new BackgroundTaskSchedulerImpl(mDelegate));
        BackgroundTaskSchedulerUma.setInstanceForTesting(mBackgroundTaskSchedulerUma);
        TestBackgroundTask.reset();
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskStartsAnytimeWithoutDeadline() {
        JobParameters jobParameters = buildJobParameters(TaskIds.TEST, null);

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskDoesNotStartExactlyAtDeadline() {
        JobParameters jobParameters = buildJobParameters(TaskIds.TEST, sClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskDoesNotStartAfterDeadline() {
        JobParameters jobParameters =
                buildJobParameters(TaskIds.TEST, sZeroClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskStartsBeforeDeadline() {
        JobParameters jobParameters = buildJobParameters(TaskIds.TEST, sClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sZeroClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelTaskIfTaskIdNotFound() {
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mBackgroundTaskSchedulerImpl);

        JobParameters jobParameters = buildJobParameters(
                TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, sClock.currentTimeMillis());

        BackgroundTaskJobService jobService = new BackgroundTaskJobService();
        jobService.setClockForTesting(sZeroClock);
        assertFalse(jobService.onStartJob(jobParameters));

        verify(mBackgroundTaskSchedulerImpl, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    private static JobParameters buildJobParameters(int taskId, Long deadlineTime) {
        PersistableBundle extras = new PersistableBundle();
        if (deadlineTime != null) {
            extras.putLong(
                    BackgroundTaskSchedulerJobService.BACKGROUND_TASK_DEADLINE_KEY, deadlineTime);
        }
        PersistableBundle taskExtras = new PersistableBundle();
        extras.putPersistableBundle(
                BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        return new JobParameters(null /* callback */, taskId, extras, null /* transientExtras */,
                null /* clipData */, 0 /* clipGrantFlags */, false /* overrideDeadlineExpired */,
                null /* triggeredContentUris */, null /* triggeredContentAuthorities */,
                null /* network */);
    }
}