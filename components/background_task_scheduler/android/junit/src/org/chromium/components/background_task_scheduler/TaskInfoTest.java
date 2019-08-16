// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TaskInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskInfoTest {
    private static final long TEST_START_MS = TimeUnit.MINUTES.toMillis(5);
    private static final long TEST_END_MS = TimeUnit.MINUTES.toMillis((10));
    private static final long TEST_FLEX_MS = 100;

    @Before
    public void setUp() {
        TestBackgroundTask.reset();
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(new TestBackgroundTaskFactory());
    }

    private void checkGeneralTaskInfoFields(TaskInfo taskInfo, int taskId) {
        assertEquals(taskId, taskInfo.getTaskId());
        assertEquals(TestBackgroundTask.class, taskInfo.getBackgroundTaskClass());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinDeadline() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowEndTimeMs(TEST_END_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        checkGeneralTaskInfoFields(oneOffTask, TaskIds.TEST);

        assertFalse(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());

        assertNotNull(oneOffTask.getOneOffInfo());
        assertNull(oneOffTask.getPeriodicInfo());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinTimeWindow() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowStartTimeMs(TEST_START_MS)
                                                 .setWindowEndTimeMs(TEST_END_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertTrue(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_START_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinZeroTimeWindow() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create()
                                                 .setWindowStartTimeMs(TEST_END_MS)
                                                 .setWindowEndTimeMs(TEST_END_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffNoParamsSet() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create().build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertFalse(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(0, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertFalse(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithInterval() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.PeriodicInfo.create()
                                                 .setIntervalMs(TEST_END_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        checkGeneralTaskInfoFields(periodicTask, TaskIds.TEST);

        assertFalse(periodicTask.getPeriodicInfo().hasFlex());
        assertEquals(TEST_END_MS, periodicTask.getPeriodicInfo().getIntervalMs());
        assertTrue(periodicTask.getPeriodicInfo().expiresAfterWindowEndTime());

        assertEquals(null, periodicTask.getOneOffInfo());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithIntervalAndFlex() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.PeriodicInfo.create()
                                                 .setIntervalMs(TEST_END_MS)
                                                 .setFlexMs(TEST_FLEX_MS)
                                                 .setExpiresAfterWindowEndTime(true)
                                                 .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertTrue(periodicTask.getPeriodicInfo().hasFlex());
        assertEquals(TEST_FLEX_MS, periodicTask.getPeriodicInfo().getFlexMs());
        assertEquals(TEST_END_MS, periodicTask.getPeriodicInfo().getIntervalMs());
        assertTrue(periodicTask.getPeriodicInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicNoParamsSet() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.PeriodicInfo.create().build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertFalse(periodicTask.getPeriodicInfo().hasFlex());
        assertEquals(0, periodicTask.getPeriodicInfo().getIntervalMs());
        assertFalse(periodicTask.getPeriodicInfo().expiresAfterWindowEndTime());
    }
}