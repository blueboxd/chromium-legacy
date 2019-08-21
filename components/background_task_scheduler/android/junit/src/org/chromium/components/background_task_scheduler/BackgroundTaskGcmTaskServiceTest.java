// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;

import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.TaskParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskGcmTaskService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskGcmTaskServiceTest {
    static TestBackgroundTaskWithParams sLastTask;
    static boolean sReturnThroughCallback;
    static boolean sNeedsRescheduling;
    private static BackgroundTaskSchedulerGcmNetworkManager.Clock sClock = () -> 1415926535000L;
    private static BackgroundTaskSchedulerGcmNetworkManager.Clock sZeroClock = () -> 0L;
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
        sReturnThroughCallback = false;
        sNeedsRescheduling = false;
        sLastTask = null;
        TestBackgroundTask.reset();
    }

    private static class TestBackgroundTaskWithParams extends TestBackgroundTask {
        private TaskParameters mTaskParameters;

        public TestBackgroundTaskWithParams() {}

        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            mTaskParameters = taskParameters;
            callback.taskFinished(sNeedsRescheduling);
            sLastTask = this;
            return sReturnThroughCallback;
        }

        public TaskParameters getTaskParameters() {
            return mTaskParameters;
        }
    }

    public class TestBackgroundTaskWithParamsFactory implements BackgroundTaskFactory {
        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            return new TestBackgroundTaskWithParams();
        }
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testStartsAnytimeWithoutDeadline() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        TaskParams taskParams = buildTaskParams(TaskIds.TEST, taskExtras, null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        assertNotNull(sLastTask);
        TaskParameters parameters = sLastTask.getTaskParameters();

        assertEquals(TaskIds.TEST, parameters.getTaskId());
        assertEquals("bar", parameters.getExtras().getString("foo"));

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testDoesNotStartExactlyAtDeadline() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams =
                buildTaskParams(TaskIds.TEST, new Bundle(), sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        assertNull(sLastTask);

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testDoesNotStartAfterDeadline() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams =
                buildTaskParams(TaskIds.TEST, new Bundle(), sZeroClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        assertNull(sLastTask);

        verify(mBackgroundTaskSchedulerUma, times(0)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testStartsBeforeDeadline() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        TaskParams taskParams =
                buildTaskParams(TaskIds.TEST, new Bundle(), sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sZeroClock);
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));

        assertNotNull(sLastTask);
        TaskParameters parameters = sLastTask.getTaskParameters();

        assertEquals(TaskIds.TEST, parameters.getTaskId());

        verify(mBackgroundTaskSchedulerUma, times(1)).reportTaskStarted(eq(TaskIds.TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOnRuntaskNeedsReschedulingFromCallback() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());
        sReturnThroughCallback = true;
        sNeedsRescheduling = true;
        TaskParams taskParams = buildTaskParams(TaskIds.TEST, new Bundle(), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_RESCHEDULE, taskService.onRunTask(taskParams));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOnRuntaskDontRescheduleFromCallback() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(
                new TestBackgroundTaskWithParamsFactory());

        sReturnThroughCallback = true;
        sNeedsRescheduling = false;
        TaskParams taskParams = buildTaskParams(TaskIds.TEST, new Bundle(), null);

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        assertEquals(GcmNetworkManager.RESULT_SUCCESS, taskService.onRunTask(taskParams));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOnInitializeTasksOnPreM() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());

        ReflectionHelpers.setStaticField(
                Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.LOLLIPOP);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        TaskInfo task = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(task);
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());

        new BackgroundTaskGcmTaskService().onInitializeTasks();
        assertEquals(1, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOnInitializeTasksOnMPlus() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());

        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", Build.VERSION_CODES.M);
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create().setWindowEndTimeMs(TimeUnit.DAYS.toMillis(1)).build();
        TaskInfo task = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(task);
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());

        new BackgroundTaskGcmTaskService().onInitializeTasks();
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCancelTaskIfTaskIdNotFound() {
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mBackgroundTaskSchedulerImpl);

        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());

        TaskParams taskParams = buildTaskParams(
                TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, new Bundle(), sClock.currentTimeMillis());

        BackgroundTaskGcmTaskService taskService = new BackgroundTaskGcmTaskService();
        taskService.setClockForTesting(sZeroClock);
        assertEquals(GcmNetworkManager.RESULT_FAILURE, taskService.onRunTask(taskParams));

        verify(mBackgroundTaskSchedulerImpl, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        assertEquals(0, TestBackgroundTask.getRescheduleCalls());
    }

    private static TaskParams buildTaskParams(int taskId, Bundle taskExtras, Long deadlineTimeMs) {
        Bundle extras = new Bundle();
        extras.putBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);
        if (deadlineTimeMs != null) {
            extras.putLong(BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_DEADLINE_KEY,
                    deadlineTimeMs);
        }

        return new TaskParams(Integer.toString(taskId), extras);
    }
}
