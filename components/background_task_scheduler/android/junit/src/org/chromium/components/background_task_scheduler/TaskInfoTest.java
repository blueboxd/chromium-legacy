// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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
    }

    private void checkGeneralTaskInfoFields(
            TaskInfo taskInfo, int taskId, Class<? extends BackgroundTask> backgroundTaskClass) {
        assertEquals(taskId, taskInfo.getTaskId());
        assertEquals(backgroundTaskClass, taskInfo.getBackgroundTaskClass());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinDeadline() {
        TaskInfo oneOffTask =
                TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class, TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();

        checkGeneralTaskInfoFields(oneOffTask, TaskIds.TEST, TestBackgroundTask.class);

        assertFalse(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());

        assertEquals(null, oneOffTask.getPeriodicInfo());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinTimeWindow() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TEST_START_MS, TEST_END_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();

        assertTrue(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_START_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinZeroTimeWindow() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TEST_END_MS, TEST_END_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();

        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithInterval() {
        TaskInfo periodicTask =
                TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class, TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();

        checkGeneralTaskInfoFields(periodicTask, TaskIds.TEST, TestBackgroundTask.class);

        assertFalse(periodicTask.getPeriodicInfo().hasFlex());
        assertEquals(TEST_END_MS, periodicTask.getPeriodicInfo().getIntervalMs());
        assertTrue(periodicTask.getPeriodicInfo().expiresAfterWindowEndTime());

        assertEquals(null, periodicTask.getOneOffInfo());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithIntervalAndFlex() {
        TaskInfo periodicTask = TaskInfo.createPeriodicTask(TaskIds.TEST, TestBackgroundTask.class,
                                                TEST_END_MS, TEST_FLEX_MS)
                                        .setExpiresAfterWindowEndTime(true)
                                        .build();

        assertTrue(periodicTask.getPeriodicInfo().hasFlex());
        assertEquals(TEST_FLEX_MS, periodicTask.getPeriodicInfo().getFlexMs());
        assertEquals(TEST_END_MS, periodicTask.getPeriodicInfo().getIntervalMs());
        assertTrue(periodicTask.getPeriodicInfo().expiresAfterWindowEndTime());
    }
}