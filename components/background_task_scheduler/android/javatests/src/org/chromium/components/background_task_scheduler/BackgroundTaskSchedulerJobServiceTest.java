// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link BackgroundTaskSchedulerJobService}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
public class BackgroundTaskSchedulerJobServiceTest {
    private static class TestBackgroundTask implements BackgroundTask {
        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            return false;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            return false;
        }

        @Override
        public void reschedule(Context context) {}
    }

    private static final long CLOCK_TIME = 1415926535000L;
    private static final long TIME_50_MIN_TO_MS = TimeUnit.MINUTES.toMillis(50);
    private static final long TIME_100_MIN_TO_MS = TimeUnit.MINUTES.toMillis(100);
    private static final long TIME_200_MIN_TO_MS = TimeUnit.MINUTES.toMillis(200);
    private static final long END_TIME_WITH_DEADLINE_MS =
            TIME_200_MIN_TO_MS + BackgroundTaskSchedulerJobService.DEADLINE_DELTA_MS;
    private static final long DEADLINE_TIME_MS = CLOCK_TIME + TIME_200_MIN_TO_MS;

    private BackgroundTaskSchedulerJobService.Clock mClock = () -> CLOCK_TIME;

    @Before
    public void setUp() {
        BackgroundTaskSchedulerJobService.setClockForTesting(mClock);
    }

    @Test
    @SmallTest
    public void testOneOffTaskWithDeadline() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TIME_200_MIN_TO_MS)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getMaxExecutionDelayMillis());
    }

    @Test
    @SmallTest
    public void testOneOffTaskWithDeadlineAndExpiration() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TIME_200_MIN_TO_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), oneOffTask);
        Assert.assertEquals(END_TIME_WITH_DEADLINE_MS, jobInfo.getMaxExecutionDelayMillis());
        Assert.assertEquals(DEADLINE_TIME_MS,
                jobInfo.getExtras().getLong(
                        BackgroundTaskSchedulerJobService.BACKGROUND_TASK_DEADLINE_KEY));
    }

    @Test
    @SmallTest
    public void testOneOffTaskWithWindow() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TIME_100_MIN_TO_MS, TIME_200_MIN_TO_MS)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        Assert.assertFalse(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_100_MIN_TO_MS, jobInfo.getMinLatencyMillis());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getMaxExecutionDelayMillis());
    }

    @Test
    @SmallTest
    public void testOneOffTaskWithWindowAndExpiration() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TIME_100_MIN_TO_MS, TIME_200_MIN_TO_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), oneOffTask);
        Assert.assertEquals(
                oneOffTask.getOneOffInfo().getWindowStartTimeMs(), jobInfo.getMinLatencyMillis());
        Assert.assertEquals(END_TIME_WITH_DEADLINE_MS, jobInfo.getMaxExecutionDelayMillis());
        Assert.assertEquals(DEADLINE_TIME_MS,
                jobInfo.getExtras().getLong(
                        BackgroundTaskSchedulerJobService.BACKGROUND_TASK_DEADLINE_KEY));
    }

    @Test
    @SmallTest
    public void testPeriodicTaskWithoutFlex() {
        TaskInfo periodicTask = TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                                TIME_200_MIN_TO_MS)
                                        .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), periodicTask);
        Assert.assertEquals(periodicTask.getTaskId(), jobInfo.getId());
        Assert.assertTrue(jobInfo.isPeriodic());
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getIntervalMillis());
    }

    @Test
    @SmallTest
    public void testPeriodicTaskWithFlex() {
        TaskInfo periodicTask = TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                                TIME_200_MIN_TO_MS, TIME_50_MIN_TO_MS)
                                        .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), periodicTask);
        Assert.assertEquals(TIME_200_MIN_TO_MS, jobInfo.getIntervalMillis());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Assert.assertEquals(TIME_50_MIN_TO_MS, jobInfo.getFlexMillis());
        }
    }

    @Test
    @SmallTest
    public void testTaskInfoWithExtras() {
        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        taskExtras.putBoolean("bools", true);
        taskExtras.putLong("longs", 1342543L);
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TIME_200_MIN_TO_MS)
                                      .setExtras(taskExtras)
                                      .build();
        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(), oneOffTask);
        Assert.assertEquals(oneOffTask.getTaskId(), jobInfo.getId());
        PersistableBundle jobExtras = jobInfo.getExtras();
        PersistableBundle persistableBundle = jobExtras.getPersistableBundle(
                BackgroundTaskSchedulerJobService.BACKGROUND_TASK_EXTRAS_KEY);
        Assert.assertEquals(taskExtras.keySet().size(), persistableBundle.keySet().size());
        Assert.assertEquals(taskExtras.getString("foo"), persistableBundle.getString("foo"));
        Assert.assertEquals(taskExtras.getBoolean("bools"), persistableBundle.getBoolean("bools"));
        Assert.assertEquals(taskExtras.getLong("longs"), persistableBundle.getLong("longs"));
    }

    @Test
    @SmallTest
    public void testTaskInfoWithManyConstraints() {
        TaskInfo.Builder taskBuilder = TaskInfo.createOneOffTask(
                TaskIds.TEST, TestBackgroundTask.class, TIME_200_MIN_TO_MS);

        JobInfo jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(),
                taskBuilder.setIsPersisted(true).build());
        Assert.assertTrue(jobInfo.isPersisted());

        jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(),
                taskBuilder.setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED).build());
        Assert.assertEquals(JobInfo.NETWORK_TYPE_UNMETERED, jobInfo.getNetworkType());

        jobInfo = BackgroundTaskSchedulerJobService.createJobInfoFromTaskInfo(
                InstrumentationRegistry.getTargetContext(),
                taskBuilder.setRequiresCharging(true).build());
        Assert.assertTrue(jobInfo.isRequireCharging());
    }
}
