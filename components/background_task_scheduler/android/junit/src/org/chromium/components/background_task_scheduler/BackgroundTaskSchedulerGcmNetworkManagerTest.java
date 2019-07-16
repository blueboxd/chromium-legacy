// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.gcm.GcmNetworkManager;
import com.google.android.gms.gcm.OneoffTask;
import com.google.android.gms.gcm.PeriodicTask;
import com.google.android.gms.gcm.Task;
import com.google.android.gms.gcm.TaskParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.gms.Shadows;
import org.robolectric.shadows.gms.common.ShadowGoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskSchedulerGcmNetworkManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowGcmNetworkManager.class, ShadowGoogleApiAvailability.class})
public class BackgroundTaskSchedulerGcmNetworkManagerTest {
    private static class TestBackgroundTaskNoPublicConstructor extends TestBackgroundTask {
        protected TestBackgroundTaskNoPublicConstructor() {}
    }

    ShadowGcmNetworkManager mGcmNetworkManager;

    private static final long CLOCK_TIME_MS = 1415926535000L;
    private static final long TIME_100_MIN_TO_MS = TimeUnit.MINUTES.toMillis(100);
    private static final long TIME_100_MIN_TO_S = TimeUnit.MINUTES.toSeconds(100);
    private static final long TIME_200_MIN_TO_MS = TimeUnit.MINUTES.toMillis(200);
    private static final long TIME_200_MIN_TO_S = TimeUnit.MINUTES.toSeconds(200);
    private static final long TIME_24_H_TO_MS = TimeUnit.HOURS.toMillis(1);
    private static final long END_TIME_WITH_DEADLINE_S =
            (TIME_200_MIN_TO_MS + BackgroundTaskSchedulerGcmNetworkManager.DEADLINE_DELTA_MS)
            / 1000;
    private static final long DEADLINE_TIME_MS = CLOCK_TIME_MS + TIME_200_MIN_TO_MS;

    private BackgroundTaskSchedulerGcmNetworkManager.Clock mClock = () -> CLOCK_TIME_MS;

    @Before
    public void setUp() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SUCCESS);
        mGcmNetworkManager = (ShadowGcmNetworkManager) Shadow.extract(
                GcmNetworkManager.getInstance(ContextUtils.getApplicationContext()));
        BackgroundTaskSchedulerGcmNetworkManager.setClockForTesting(mClock);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithDeadline() {
        TaskInfo oneOffTaskInfo = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                                  TIME_200_MIN_TO_MS)
                                          .build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(TIME_200_MIN_TO_S, oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithDeadlineAndExpiration() {
        TaskInfo oneOffTaskInfo = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                                  TIME_200_MIN_TO_MS)
                                          .setExpiresAfterWindowEndTime(true)
                                          .build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(END_TIME_WITH_DEADLINE_S, oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
        assertEquals(DEADLINE_TIME_MS,
                task.getExtras().getLong(
                        BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_DEADLINE_KEY));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithWindow() {
        TaskInfo oneOffTaskInfo = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                                  TIME_100_MIN_TO_MS, TIME_200_MIN_TO_MS)
                                          .build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(TIME_200_MIN_TO_S, oneOffTask.getWindowEnd());
        assertEquals(TIME_100_MIN_TO_S, oneOffTask.getWindowStart());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffTaskWithWindowAndExpiration() {
        TaskInfo oneOffTaskInfo = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                                  TIME_100_MIN_TO_MS, TIME_200_MIN_TO_MS)
                                          .setExpiresAfterWindowEndTime(true)
                                          .build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertEquals(END_TIME_WITH_DEADLINE_S, oneOffTask.getWindowEnd());
        assertEquals(TIME_100_MIN_TO_S, oneOffTask.getWindowStart());
        assertEquals(DEADLINE_TIME_MS,
                task.getExtras().getLong(
                        BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_DEADLINE_KEY));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskWithoutFlex() {
        TaskInfo periodicTaskInfo = TaskInfo.createPeriodicTask(TaskIds.TEST,
                                                    TestBackgroundTask.class, TIME_200_MIN_TO_MS)
                                            .build();
        Task task =
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(periodicTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof PeriodicTask);
        PeriodicTask periodicTask = (PeriodicTask) task;
        assertEquals(TIME_200_MIN_TO_S, periodicTask.getPeriod());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicTaskWithFlex() {
        TaskInfo periodicTaskInfo =
                TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                TIME_200_MIN_TO_MS, TimeUnit.MINUTES.toMillis(50))
                        .build();
        Task task =
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(periodicTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof PeriodicTask);
        PeriodicTask periodicTask = (PeriodicTask) task;
        assertEquals(TIME_200_MIN_TO_S, periodicTask.getPeriod());
        assertEquals(TimeUnit.MINUTES.toSeconds(50), periodicTask.getFlex());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskInfoWithExtras() {
        Bundle userExtras = new Bundle();
        userExtras.putString("foo", "bar");
        userExtras.putBoolean("bools", true);
        userExtras.putLong("longs", 1342543L);
        TaskInfo oneOffTaskInfo = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                                  TIME_200_MIN_TO_MS)
                                          .setExtras(userExtras)
                                          .build();
        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo);
        assertEquals(Integer.toString(TaskIds.TEST), task.getTag());
        assertTrue(task instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) task;

        Bundle taskExtras = task.getExtras();
        Bundle bundle = taskExtras.getBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY);
        assertEquals(userExtras.keySet().size(), bundle.keySet().size());
        assertEquals(userExtras.getString("foo"), bundle.getString("foo"));
        assertEquals(userExtras.getBoolean("bools"), bundle.getBoolean("bools"));
        assertEquals(userExtras.getLong("longs"), bundle.getLong("longs"));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testTaskInfoWithManyConstraints() {
        TaskInfo.Builder taskBuilder = TaskInfo.createOneOffTask(
                TaskIds.TEST, TestBackgroundTask.class, TIME_200_MIN_TO_MS);

        Task task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setIsPersisted(true).build());
        assertTrue(task.isPersisted());

        task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED).build());
        assertEquals(Task.NETWORK_STATE_UNMETERED, task.getRequiredNetwork());

        task = BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(
                taskBuilder.setRequiresCharging(true).build());
        assertTrue(task.getRequiresCharging());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testGetTaskParametersFromTaskParams() {
        Bundle extras = new Bundle();
        Bundle taskExtras = new Bundle();
        taskExtras.putString("foo", "bar");
        extras.putBundle(
                BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_EXTRAS_KEY, taskExtras);

        TaskParams params = new TaskParams(Integer.toString(TaskIds.TEST), extras);

        TaskParameters parameters =
                BackgroundTaskSchedulerGcmNetworkManager.getTaskParametersFromTaskParams(params);
        assertEquals(TaskIds.TEST, parameters.getTaskId());
        assertEquals("bar", parameters.getExtras().getString("foo"));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testGetBackgroundTaskFromTaskParams() {
        Bundle extras = new Bundle();
        extras.putString(BackgroundTaskSchedulerGcmNetworkManager.BACKGROUND_TASK_CLASS_KEY,
                TestBackgroundTask.class.getName());

        TaskParams params = new TaskParams(Integer.toString(TaskIds.TEST), extras);
        BackgroundTask backgroundTask =
                BackgroundTaskSchedulerGcmNetworkManager.getBackgroundTaskFromTaskParams(params);

        assertNotNull(backgroundTask);
        assertTrue(backgroundTask instanceof TestBackgroundTask);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testSchedule() {
        TaskInfo oneOffTaskInfo =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class, TIME_24_H_TO_MS)
                        .build();

        assertNull(mGcmNetworkManager.getScheduledTask());

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        assertTrue(delegate.schedule(ContextUtils.getApplicationContext(), oneOffTaskInfo));

        // Check that a task was scheduled using GCM Network Manager.
        assertNotNull(mGcmNetworkManager.getScheduledTask());

        // Verify details of the scheduled task.
        Task scheduledTask = mGcmNetworkManager.getScheduledTask();
        assertTrue(scheduledTask instanceof OneoffTask);
        OneoffTask oneOffTask = (OneoffTask) scheduledTask;

        assertEquals(Integer.toString(TaskIds.TEST), scheduledTask.getTag());
        assertEquals(TimeUnit.HOURS.toSeconds(1), oneOffTask.getWindowEnd());
        assertEquals(0, oneOffTask.getWindowStart());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testScheduleNoPublicConstructor() {
        TaskInfo oneOffTaskInfo =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTaskNoPublicConstructor.class,
                                TIME_24_H_TO_MS)
                        .build();

        assertFalse(new BackgroundTaskSchedulerGcmNetworkManager().schedule(
                ContextUtils.getApplicationContext(), oneOffTaskInfo));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testScheduleNoGooglePlayServices() {
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SERVICE_MISSING);

        TaskInfo oneOffTaskInfo =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class, TIME_24_H_TO_MS)
                        .build();

        assertFalse(new BackgroundTaskSchedulerGcmNetworkManager().schedule(
                ContextUtils.getApplicationContext(), oneOffTaskInfo));
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testCancel() {
        TaskInfo oneOffTaskInfo =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class, TIME_24_H_TO_MS)
                        .build();

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        assertTrue(delegate.schedule(ContextUtils.getApplicationContext(), oneOffTaskInfo));
        delegate.cancel(ContextUtils.getApplicationContext(), TaskIds.TEST);

        Task canceledTask = mGcmNetworkManager.getCanceledTask();
        assertEquals(Integer.toString(TaskIds.TEST), canceledTask.getTag());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testCancelNoGooglePlayServices() {
        // This simulates situation where Google Play Services is uninstalled.
        Shadows.shadowOf(GoogleApiAvailability.getInstance())
                .setIsGooglePlayServicesAvailable(ConnectionResult.SERVICE_MISSING);

        TaskInfo oneOffTaskInfo =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class, TIME_24_H_TO_MS)
                        .build();

        // Ensure there was a previously scheduled task.
        mGcmNetworkManager.schedule(
                BackgroundTaskSchedulerGcmNetworkManager.createTaskFromTaskInfo(oneOffTaskInfo));

        BackgroundTaskSchedulerDelegate delegate = new BackgroundTaskSchedulerGcmNetworkManager();

        // This call is expected to have no effect on GCM Network Manager, because Play Services are
        // not available.
        delegate.cancel(ContextUtils.getApplicationContext(), TaskIds.TEST);
        assertNull(mGcmNetworkManager.getCanceledTask());
        assertNotNull(mGcmNetworkManager.getScheduledTask());
    }
}
