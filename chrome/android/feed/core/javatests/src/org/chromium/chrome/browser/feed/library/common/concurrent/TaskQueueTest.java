// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.common.util.concurrent.MoreExecutors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link TaskQueue} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskQueueTest {
    private static final long LONG_RUNNING_TASK_TIME = 123456L;
    private final FakeBasicLoggingApi mFakeBasicLoggingApi = new FakeBasicLoggingApi();
    private final FakeClock mFakeClock = new FakeClock();
    private final List<Integer> mCallOrder = new ArrayList<>();
    private final FakeMainThreadRunner mFakeMainThreadRunner =
            FakeMainThreadRunner.create(mFakeClock);
    private final TaskQueue mTaskQueue = new TaskQueue(mFakeBasicLoggingApi,
            MoreExecutors.directExecutor(), mFakeMainThreadRunner, mFakeClock);

    private boolean mDelayedTaskHasRun;
    private boolean mDelayedTaskHasTimedOut;

    @Before
    public void setUp() {
        initMocks(this);
        mCallOrder.clear();
    }

    @Test
    public void testInitialization() {
        assertThat(mTaskQueue.isDelayed()).isTrue();
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testInitializationCallOrder() {
        Runnable background = () -> runnable(1);
        Runnable userFacing = () -> runnable(2);
        Runnable postInit = () -> runnable(3);
        Runnable init = () -> runnable(4);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        assertThat(mTaskQueue.hasBacklog()).isTrue();
        mTaskQueue.initialize(init);

        assertThat(mTaskQueue.hasBacklog()).isFalse();
        assertOrder(4, 3, 2, 1);
    }

    @Test
    public void testPostInit() {
        mTaskQueue.initialize(this::noOp);

        Runnable invalidate = () -> runnable(1);
        Runnable postInit = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable reset = () -> runnable(4);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidate);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, reset);

        assertThat(mTaskQueue.hasBacklog()).isFalse();
        assertOrder(1, 2, 4, 3);
    }

    @Test
    public void testHeadInvalidateReset() {
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(mTaskQueue.isDelayed()).isTrue();
        assertThat(mDelayedTaskHasRun).isFalse();
        assertThat(mTaskQueue.hasPendingStarvationCheck()).isTrue();

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);

        assertThat(mTaskQueue.isDelayed()).isFalse();
        assertThat(mDelayedTaskHasRun).isTrue();
        assertThat(mTaskQueue.hasPendingStarvationCheck()).isFalse();
    }

    @Test
    public void testHeadInvalidateDropMultiple() {
        Runnable invalidate = () -> runnable(1);
        Runnable invalidateNotRun = () -> runnable(2);
        Runnable init = () -> runnable(3);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidate);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, invalidateNotRun);
        assertThat(mTaskQueue.hasBacklog()).isTrue();
        mTaskQueue.initialize(init);

        assertThat(mTaskQueue.hasBacklog()).isFalse();
        assertOrder(3, 1);
    }

    @Test
    public void testHeadInvalidateCallOrder() {
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();

        Runnable headInvalidate = () -> runnable(1);
        Runnable background = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable postInit = () -> runnable(4);
        Runnable headReset = () -> runnable(5);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, headInvalidate);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit); // run immediately
        assertThat(mTaskQueue.hasBacklog()).isTrue();
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, headReset);

        assertThat(mTaskQueue.hasBacklog()).isFalse();
        assertOrder(1, 4, 5, 3, 2);
    }

    @Test
    public void testResetTaskQueue_immediate() {
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();

        mTaskQueue.reset();
        assertThat(mTaskQueue.isDelayed()).isTrue();
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testResetQueue_withDelay() {
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();

        Runnable headInvalidate = () -> runnable(1);
        Runnable background = () -> runnable(2);
        Runnable userFacing = () -> runnable(3);
        Runnable postInit = () -> runnable(4);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, headInvalidate);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, background);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.USER_FACING, userFacing);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, postInit);
        assertThat(mTaskQueue.hasBacklog()).isTrue();

        mTaskQueue.reset();
        assertThat(mTaskQueue.isDelayed()).isTrue();
        assertThat(mTaskQueue.hasBacklog()).isFalse();
        mTaskQueue.initialize(this::noOp);
        assertThat(mTaskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testStarvation_initialization() {
        mTaskQueue.initialize(this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(mDelayedTaskHasRun).isTrue();
        assertThat(mTaskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testStarvation_headInvalidate() {
        mTaskQueue.initialize(this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(mFakeMainThreadRunner.getCompletedTaskCount()).isEqualTo(0);
        assertThat(mDelayedTaskHasRun).isFalse();
        assertThat(mTaskQueue.isDelayed()).isTrue();

        runAndAssertStarvationChecks();
        assertThat(mDelayedTaskHasRun).isTrue();
        assertThat(mTaskQueue.isDelayed()).isFalse();
        assertThat(mFakeBasicLoggingApi.lastInternalError)
                .isEqualTo(InternalFeedError.TASK_QUEUE_STARVATION);
    }

    @Test
    public void testStarvation_reset() {
        mTaskQueue.initialize(this::noOp);
        mTaskQueue.reset();
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask);

        assertThat(mDelayedTaskHasRun).isFalse();
        assertThat(mTaskQueue.isDelayed()).isTrue();

        runAndAssertStarvationChecks();
        assertThat(mDelayedTaskHasRun).isTrue();
        assertThat(mTaskQueue.isDelayed()).isFalse();
    }

    @Test
    public void testTaskTiming_immediatelyRunTask_logsTaskFinished() {
        mTaskQueue.initialize(this::noOp);

        mTaskQueue.execute(Task.CLEAR_ALL, TaskType.IMMEDIATE, this::noOp);

        assertThat(mFakeBasicLoggingApi.lastTaskDelay).isEqualTo(0);
        assertThat(mFakeBasicLoggingApi.lastTaskTime).isEqualTo(0);
        assertThat(mFakeBasicLoggingApi.lastTaskLogged).isEqualTo(Task.CLEAR_ALL);
    }

    @Test
    public void testTaskTiming_longTask_logsTaskFinished() {
        mTaskQueue.initialize(this::noOp);

        mTaskQueue.execute(Task.UNKNOWN, TaskType.IMMEDIATE, this::longRunningTask);

        assertThat(mFakeBasicLoggingApi.lastTaskTime).isEqualTo(LONG_RUNNING_TASK_TIME);
    }

    @Test
    public void testTaskTiming_delayedTask_logsTaskFinished() {
        // Queue task before initialization, causing it to be delayed
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::delayedTask);
        long delayTime = 123L;
        mFakeClock.advance(delayTime);

        // Initialize, allowing the delayed task to be run
        mTaskQueue.initialize(this::noOp);

        assertThat(mFakeBasicLoggingApi.lastTaskDelay).isEqualTo(delayTime);
    }

    @Test
    public void testTimeout_withoutTimeout() {
        // Put the TaskQueue into a delaying state and schedule a task with a timeout.
        mTaskQueue.initialize(this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask,
                this::delayedTaskTimeout,
                /*timeoutMillis= */ 10L);

        mFakeClock.advance(9L);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);
        assertThat(mDelayedTaskHasRun).isTrue();
        assertThat(mDelayedTaskHasTimedOut).isFalse();

        mFakeClock.tick();
        assertThat(mDelayedTaskHasTimedOut).isFalse();
    }

    @Test
    public void testTimeout_withTimeout() {
        // Put the TaskQueue into a delaying state and schedule a task with a timeout.
        mTaskQueue.initialize(this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_INVALIDATE, this::noOp);
        mTaskQueue.execute(Task.UNKNOWN, TaskType.BACKGROUND, this::delayedTask,
                this::delayedTaskTimeout,
                /*timeoutMillis= */ 10L);

        mFakeClock.advance(10L);
        assertThat(mDelayedTaskHasRun).isFalse();
        assertThat(mDelayedTaskHasTimedOut).isTrue();

        mTaskQueue.execute(Task.UNKNOWN, TaskType.HEAD_RESET, this::noOp);
        assertThat(mDelayedTaskHasRun).isFalse();
    }

    private void runAndAssertStarvationChecks() {
        int starvationTaskCount = 0;
        long startTimeMillis = mFakeClock.currentTimeMillis();
        while (mFakeClock.currentTimeMillis() - startTimeMillis < TaskQueue.STARVATION_TIMEOUT_MS) {
            assertThat(mTaskQueue.hasPendingStarvationCheck()).isTrue();
            mFakeClock.advance(TaskQueue.STARVATION_CHECK_MS);
            starvationTaskCount++;
            assertThat(mFakeMainThreadRunner.getCompletedTaskCount())
                    .isEqualTo(starvationTaskCount);
        }
        assertThat(mTaskQueue.hasPendingStarvationCheck()).isFalse();
    }

    private void assertOrder(Integer... args) {
        assertThat(mCallOrder.size()).isEqualTo(args.length);
        for (int i = 0; i < args.length; i++) {
            assertThat(mCallOrder.get(i)).isEqualTo(args[i]);
        }
    }

    private void runnable(int taskId) {
        mCallOrder.add(taskId);
    }

    private void noOp() {}

    private void delayedTask() {
        mDelayedTaskHasRun = true;
    }

    private void delayedTaskTimeout() {
        mDelayedTaskHasTimedOut = true;
    }

    private void longRunningTask() {
        mFakeClock.advance(LONG_RUNNING_TASK_TIME);
    }
}
