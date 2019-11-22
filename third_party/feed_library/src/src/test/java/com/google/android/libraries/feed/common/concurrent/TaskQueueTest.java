// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.common.util.concurrent.MoreExecutors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link TaskQueue} class. */
@RunWith(RobolectricTestRunner.class)
public class TaskQueueTest {
    private static final long LONG_RUNNING_TASK_TIME = 123456L;
    private final FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();
    private final FakeClock fakeClock = new FakeClock();
    private final List<Integer> callOrder = new ArrayList<>();
    private final FakeMainThreadRunner fakeMainThreadRunner =
            FakeMainThreadRunner.create(fakeClock);
    private final TaskQueue taskQueue = new TaskQueue(
            fakeBasicLoggingApi, MoreExecutors.directExecutor(), fakeMainThreadRunner, fakeClock);

    private boolean delayedTaskHasRun;
    private boolean delayedTaskHasTimedOut;

    @Before
    public void setUp() {
        initMocks(this);
        callOrder.clear();
    }

    @Test
    public void testInitialization() {
        assertThat(taskQueue.isDelayed()).isTrue();
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testInitializationCallOrder() {
        Runnable background = () -> runnable(1);
        Runnable userFacing = () -> runnable(2);
        Runnable postInit = () -> runnable(3);
        Runnable init = () -> runnable(4);

        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        taskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        taskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        assertThat(taskQueue.hasBacklog()).isTrue();
        taskQueue.initialize(init);

        assertThat(taskQueue.hasBacklog()).isFalse();
        assertOrder(4, 3, 2, 1);
    }

    @Test
    public void testPostInit() {
        taskQueue.initialize(this::noOp);

        Runnable invalidate = () -> runnable(1);
        Runnable postInit = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable reset = () -> runnable(4);

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidate);
        taskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        taskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, reset);

        assertThat(taskQueue.hasBacklog()).isFalse();
        assertOrder(1, 2, 4, 3);
    }

    @Test
    public void testHeadInvalidateReset() {
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(taskQueue.isDelayed()).isTrue();
        assertThat(delayedTaskHasRun).isFalse();
        assertThat(taskQueue.hasPendingStarvationCheck()).isTrue();

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);

        assertThat(taskQueue.isDelayed()).isFalse();
        assertThat(delayedTaskHasRun).isTrue();
        assertThat(taskQueue.hasPendingStarvationCheck()).isFalse();
    }

    @Test
    public void testHeadInvalidateDropMultiple() {
        Runnable invalidate = () -> runnable(1);
        Runnable invalidateNotRun = () -> runnable(2);
        Runnable init = () -> runnable(3);

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidate);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidateNotRun);
        assertThat(taskQueue.hasBacklog()).isTrue();
        taskQueue.initialize(init);

        assertThat(taskQueue.hasBacklog()).isFalse();
        assertOrder(3, 1);
    }

    @Test
    public void testHeadInvalidateCallOrder() {
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();

        Runnable headInvalidate = () -> runnable(1);
        Runnable background = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable postInit = () -> runnable(4);
        Runnable headReset = () -> runnable(5);

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, headInvalidate);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        taskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        taskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit); // run immediately
        assertThat(taskQueue.hasBacklog()).isTrue();
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, headReset);

        assertThat(taskQueue.hasBacklog()).isFalse();
        assertOrder(1, 4, 5, 3, 2);
    }

    @Test
    public void testResetTaskQueue_immediate() {
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();

        taskQueue.reset();
        assertThat(taskQueue.isDelayed()).isTrue();
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testResetQueue_withDelay() {
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();

        Runnable headInvalidate = () -> runnable(1);
        Runnable background = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable postInit = () -> runnable(4);

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, headInvalidate);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        taskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        taskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        assertThat(taskQueue.hasBacklog()).isTrue();

        taskQueue.reset();
        assertThat(taskQueue.isDelayed()).isTrue();
        assertThat(taskQueue.hasBacklog()).isFalse();
        taskQueue.initialize(this::noOp);
        assertThat(taskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testStarvation_initialization() {
        taskQueue.initialize(this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(delayedTaskHasRun).isTrue();
        assertThat(taskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testStarvation_headInvalidate() {
        taskQueue.initialize(this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(fakeMainThreadRunner.getCompletedTaskCount()).isEqualTo(0);
        assertThat(delayedTaskHasRun).isFalse();
        assertThat(taskQueue.isDelayed()).isTrue();

        runAndAssertStarvationChecks();
        assertThat(delayedTaskHasRun).isTrue();
        assertThat(taskQueue.isDelayed()).isFalse();
        assertThat(fakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.TASK_QUEUE_STARVATION);
    }

    @Test
    public void testStarvation_reset() {
        taskQueue.initialize(this::noOp);
        taskQueue.reset();
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(delayedTaskHasRun).isFalse();
        assertThat(taskQueue.isDelayed()).isTrue();

        runAndAssertStarvationChecks();
        assertThat(delayedTaskHasRun).isTrue();
        assertThat(taskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testTaskTiming_immediatelyRunTask_logsTaskFinished() {
        taskQueue.initialize(this::noOp);

        taskQueue.execute(Task.CLEAR_ALL, TaskType.IMMEDIATE, this::noOp);

        assertThat(fakeBasicLoggingApi.lastTaskDelay).isEqualTo(0);
        assertThat(fakeBasicLoggingApi.lastTaskTime).isEqualTo(0);
        assertThat(fakeBasicLoggingApi.lastTaskLogged).isEqualTo(Task.CLEAR_ALL);
    }

    @Test
    public void testTaskTiming_longTask_logsTaskFinished() {
        taskQueue.initialize(this::noOp);

        taskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, this::longRunningTask);

        assertThat(fakeBasicLoggingApi.lastTaskTime).isEqualTo(LONG_RUNNING_TASK_TIME);
    }

    @Test
    public void testTaskTiming_delayedTask_logsTaskFinished() {
        // Queue task before initialization, causing it to be delayed
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::delayedTask);
        long delayTime = 123L;
        fakeClock.advance(delayTime);

        // Initialize, allowing the delayed task to be run
        taskQueue.initialize(this::noOp);

        assertThat(fakeBasicLoggingApi.lastTaskDelay).isEqualTo(delayTime);
    }

    @Test
    public void testTimeout_withoutTimeout() {
        // Put the TaskQueue into a delaying state and schedule a task with a timeout.
        taskQueue.initialize(this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask,
                this::delayedTaskTimeout,
                /*timeoutMillis= */ 10L);

        fakeClock.advance(9L);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);
        assertThat(delayedTaskHasRun).isTrue();
        assertThat(delayedTaskHasTimedOut).isFalse();

        fakeClock.tick();
        assertThat(delayedTaskHasTimedOut).isFalse();
    }

    @Test
    public void testTimeout_withTimeout() {
        // Put the TaskQueue into a delaying state and schedule a task with a timeout.
        taskQueue.initialize(this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        taskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask,
                this::delayedTaskTimeout,
                /*timeoutMillis= */ 10L);

        fakeClock.advance(10L);
        assertThat(delayedTaskHasRun).isFalse();
        assertThat(delayedTaskHasTimedOut).isTrue();

        taskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);
        assertThat(delayedTaskHasRun).isFalse();
    }

    private void runAndAssertStarvationChecks() {
        int starvationTaskCount = 0;
        long startTimeMillis = fakeClock.currentTimeMillis();
        while (fakeClock.currentTimeMillis() - startTimeMillis < TaskQueue.STARVATION_TIMEOUT_MS) {
            assertThat(taskQueue.hasPendingStarvationCheck()).isTrue();
            fakeClock.advance(TaskQueue.STARVATION_CHECK_MS);
            starvationTaskCount++;
            assertThat(fakeMainThreadRunner.getCompletedTaskCount()).isEqualTo(starvationTaskCount);
        }
        assertThat(taskQueue.hasPendingStarvationCheck()).isFalse();
    }

    private void assertOrder(Integer... args) {
        assertThat(callOrder.size()).isEqualTo(args.length);
        for (int i = 0; i < args.length; i++) {
            assertThat(callOrder.get(i)).isEqualTo(args[i]);
        }
    }

    private void runnable(int taskId) {
        callOrder.add(taskId);
    }

    private void noOp() {}

    private void delayedTask() {
        delayedTaskHasRun = true;
    }

    private void delayedTaskTimeout() {
        delayedTaskHasTimedOut = true;
    }

    private void longRunningTask() {
        fakeClock.advance(LONG_RUNNING_TASK_TIME);
    }
}
