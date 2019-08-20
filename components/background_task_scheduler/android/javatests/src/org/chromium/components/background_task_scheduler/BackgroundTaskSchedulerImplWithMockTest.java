// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link BackgroundTaskSchedulerImpl}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BackgroundTaskSchedulerImplWithMockTest {
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

    private static final int TEST_MINUTES = 10;

    @Test
    @SmallTest
    public void testOneOffTaskScheduling() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                                      .build();

        MockBackgroundTaskSchedulerDelegate delegate = new MockBackgroundTaskSchedulerDelegate();
        BackgroundTaskScheduler taskScheduler = new BackgroundTaskSchedulerImpl(delegate);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(taskScheduler.schedule(null, oneOffTask)); });

        Assert.assertEquals(oneOffTask, delegate.getScheduledTaskInfo());
        Assert.assertEquals(0, delegate.getCanceledTaskId());
    }

    @Test
    @SmallTest
    public void testPeriodicTaskScheduling() {
        TaskInfo periodicTask = TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                                TimeUnit.MINUTES.toMillis(TEST_MINUTES))
                                        .build();

        MockBackgroundTaskSchedulerDelegate delegate = new MockBackgroundTaskSchedulerDelegate();
        BackgroundTaskScheduler taskScheduler = new BackgroundTaskSchedulerImpl(delegate);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(taskScheduler.schedule(null, periodicTask)); });

        Assert.assertEquals(periodicTask, delegate.getScheduledTaskInfo());
        Assert.assertEquals(0, delegate.getCanceledTaskId());
    }

    @Test
    @SmallTest
    public void testTaskCanceling() {
        MockBackgroundTaskSchedulerDelegate delegate = new MockBackgroundTaskSchedulerDelegate();
        BackgroundTaskScheduler taskScheduler = new BackgroundTaskSchedulerImpl(delegate);

        TestThreadUtils.runOnUiThreadBlocking(() -> { taskScheduler.cancel(null, TaskIds.TEST); });

        Assert.assertEquals(null, delegate.getScheduledTaskInfo());
        Assert.assertEquals(TaskIds.TEST, delegate.getCanceledTaskId());
    }
}