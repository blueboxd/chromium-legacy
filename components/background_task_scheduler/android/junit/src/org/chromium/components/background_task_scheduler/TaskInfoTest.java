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

/** Unit tests for {@link TaskInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskInfoTest {
    private static final int TEST_START_MS = 300000;
    private static final int TEST_END_MS = 600000;

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
    public void testExpirationWithinDeadline() {
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
    public void testExpirationWithinTimeWindow() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TEST_START_MS, TEST_END_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();

        checkGeneralTaskInfoFields(oneOffTask, TaskIds.TEST, TestBackgroundTask.class);

        assertTrue(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_START_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());

        assertEquals(null, oneOffTask.getPeriodicInfo());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testExpirationWithinZeroTimeWindow() {
        TaskInfo oneOffTask = TaskInfo.createOneOffTask(TaskIds.TEST, TestBackgroundTask.class,
                                              TEST_END_MS, TEST_END_MS)
                                      .setExpiresAfterWindowEndTime(true)
                                      .build();

        checkGeneralTaskInfoFields(oneOffTask, TaskIds.TEST, TestBackgroundTask.class);

        assertTrue(oneOffTask.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(TEST_END_MS, oneOffTask.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(oneOffTask.getOneOffInfo().expiresAfterWindowEndTime());

        assertEquals(null, oneOffTask.getPeriodicInfo());
    }
}