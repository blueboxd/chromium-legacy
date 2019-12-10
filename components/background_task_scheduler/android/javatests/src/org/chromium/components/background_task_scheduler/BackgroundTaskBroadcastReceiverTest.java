// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.Intent;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/** Tests for {@link BackgroundTaskBroadcastReceiver}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class BackgroundTaskBroadcastReceiverTest {
    class TestBackgroundTask implements BackgroundTask {
        public TestBackgroundTask() {}

        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            ThreadUtils.assertOnUiThread();
            mScheduled++;
            return false;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            ThreadUtils.assertOnUiThread();
            mStopped++;
            return false;
        }

        @Override
        public void reschedule(Context context) {
            ThreadUtils.assertOnUiThread();
            mRescheduled++;
        }
    }

    class TestBackgroundTaskFactory implements BackgroundTaskFactory {
        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            if (taskId == TaskIds.TEST) {
                return new TestBackgroundTask();
            }
            return null;
        }
    }

    private int mScheduled;
    private int mStopped;
    private int mRescheduled;

    @Before
    public void setUp() {
        mScheduled = 0;
        mStopped = 0;
        mRescheduled = 0;

        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());
    }

    @Test
    @MediumTest
    public void testStartExact() {
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Callable<Boolean> hasScheduled = () -> mScheduled == 1;
        CriteriaHelper.pollUiThread(hasScheduled, "Failed to schedule task",
                /*maxTimeoutMs=*/TimeUnit.MINUTES.toMillis(2), /*checkIntervalMs=*/50);

        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }
}
