// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent;

import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.logging.StringFormattingUtils;
import com.google.android.libraries.feed.common.time.Clock;

import java.util.ArrayDeque;
import java.util.Queue;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

import javax.annotation.concurrent.GuardedBy;

/**
 * This class is responsible for running tasks on the Feed single-threaded Executor. The primary job
 * of this class is to run high priority tasks and to delay certain task until other complete. When
 * we are delaying tasks, they will be added to a set of queues and will run in order within the
 * task priority. There are three priorities of tasks defined:
 *
 * <ol>
 *   <li>Initialization, HEAD_INVALIDATE, HEAD_RESET - These tasks will be placed on the Executor
 *       when they are received.
 *   <li>USER_FACING - These tasks are high priority, running after the immediate tasks.
 *   <li>BACKGROUND - These are low priority tasks which run after all other tasks finish.
 * </ol>
 *
 * <p>The {@code TaskQueue} start in initialization mode. All tasks will be delayed until we
 * initialization is completed. The {@link #initialize(Runnable)} method is run to initialize the
 * FeedSessionManager. We also enter delayed mode when we either reset the $HEAD or invalidate the
 * $HEAD. For HEAD_RESET, we are making a request which will complete. Once it's complete, we will
 * process any delayed tasks. HEAD_INVALIDATE simply clears the contents of $HEAD. The expectation
 * is a future HEAD_RESET will populate $HEAD. Once the delay is cleared, we will run the
 * USER_FACING tasks followed by the BACKGROUND tasks. Once all of these tasks have run, we will run
 * tasks immediately until we either have a task which is of type HEAD_INVALIDATE or HEAD_RESET.
 */
// TODO: This class should be final for Tiktok conformance
public class TaskQueue implements Dumpable {
    private static final String TAG = "TaskQueue";

    /**
     * Once we delay the queue, if we are not making any progress after the initial {@code
     * #STARVATION_TIMEOUT_MS}, we will start running tasks. {@link #STARVATION_CHECK_MS} is the
     * amount of time until we check for starvation. Checking for starvation is done on the main
     * thread. Starvation checks are started when we initially delay the queue and only runs while
     * the queue is delayed.
     */
    @VisibleForTesting
    public static final long STARVATION_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(15);

    @VisibleForTesting
    static final long STARVATION_CHECK_MS = TimeUnit.SECONDS.toMillis(6);

    /** TaskType identifies the type of task being run and implicitly the priority of the task */
    @IntDef({
            TaskType.UNKNOWN,
            TaskType.IMMEDIATE,
            TaskType.HEAD_INVALIDATE,
            TaskType.HEAD_RESET,
            TaskType.USER_FACING,
            TaskType.BACKGROUND,
    })
    public @interface TaskType {
        // Unknown task priority, shouldn't be used, this will be treated as a background task
        int UNKNOWN = 0;
        // Runs immediately.  IMMEDIATE tasks will wait for initialization to be finished
        int IMMEDIATE = 1;
        // Runs immediately, $HEAD is invalidated (cleared) and delay tasks until HEAD_RESET
        int HEAD_INVALIDATE = 2;
        // Runs immediately, indicates the task will create a new $HEAD instance.
        // Once finished, start running other tasks until the delayed tasks are all run.
        int HEAD_RESET = 3;
        // User facing task which should run at a higher priority than Background.
        int USER_FACING = 4;
        // Background tasks run at the lowest priority
        int BACKGROUND = 5;
    }

    private final Object lock = new Object();

    @GuardedBy("lock")
    private final Queue<TaskWrapper> immediateTasks = new ArrayDeque<>();

    @GuardedBy("lock")
    private final Queue<TaskWrapper> userTasks = new ArrayDeque<>();

    @GuardedBy("lock")
    private final Queue<TaskWrapper> backgroundTasks = new ArrayDeque<>();

    @GuardedBy("lock")
    private boolean waitingForHeadReset;

    @GuardedBy("lock")
    private boolean initialized;

    /**
     * CancelableTask that tracks the current starvation runnable. {@liternal null} means that
     * starvation checks are not running.
     */
    @GuardedBy("lock")
    /*@Nullable*/
    private CancelableTask starvationCheckTask;

    // Tracks the current task running on the executor
    /*@Nullable*/ private TaskWrapper currentTask;

    /** Track the time the last task finished. Used for Starvation checks. */
    private final AtomicLong lastTaskFinished = new AtomicLong();

    private final BasicLoggingApi basicLoggingApi;
    private final Executor executor;
    private final Clock clock;
    private final MainThreadRunner mainThreadRunner;

    // counters used for dump
    protected int taskCount;
    protected int immediateRunCount;
    protected int delayedRunCount;
    protected int immediateTaskCount;
    protected int headInvalidateTaskCount;
    protected int headResetTaskCount;
    protected int userFacingTaskCount;
    protected int backgroundTaskCount;
    protected int maxImmediateTasks;
    protected int maxUserFacingTasks;
    protected int maxBackgroundTasks;

    public TaskQueue(BasicLoggingApi basicLoggingApi, Executor executor,
            MainThreadRunner mainThreadRunner, Clock clock) {
        this.basicLoggingApi = basicLoggingApi;
        this.executor = executor;
        this.mainThreadRunner = mainThreadRunner;
        this.clock = clock;
    }

    /** Returns {@code true} if we are delaying for a request */
    public boolean isMakingRequest() {
        synchronized (lock) {
            return waitingForHeadReset;
        }
    }

    /**
     * This method will reset the task queue. This means we will delay all tasks created until the
     * {@link #initialize(Runnable)} task is called again. Also any currently delayed tasks will be
     * removed from the Queue and not run.
     */
    public void reset() {
        synchronized (lock) {
            waitingForHeadReset = false;
            initialized = false;

            // clear all delayed tasks
            Logger.i(TAG, " - Reset i: %s, u: %s, b: %s", immediateTasks.size(), userTasks.size(),
                    backgroundTasks.size());
            immediateTasks.clear();
            userTasks.clear();
            backgroundTasks.clear();

            // Since we are delaying thing, start the starvation checker
            startStarvationCheck();
        }
    }

    /** this is called post reset to clear the initialized flag. */
    public void completeReset() {
        Logger.i(TAG, "completeReset");
        synchronized (lock) {
            initialized = true;
        }

        maybeCancelStarvationCheck();
    }

    /**
     * Called to initialize the {@link FeedSessionManager}. This needs to be the first task run, all
     * other tasks are delayed until initialization finishes.
     */
    public void initialize(Runnable runnable) {
        synchronized (lock) {
            if (initialized) {
                Logger.w(TAG, " - Calling initialize on an initialized TaskQueue");
            }
        }

        TaskWrapper task = new InitializationTaskWrapper(runnable);
        countTask(task.taskType);
        task.runTask();
    }

    /** Execute a Task on the Executor. */
    public void execute(@Task int task, @TaskType int taskType, Runnable runnable) {
        execute(task, taskType, runnable, null, 0);
    }

    /** Execute a task providing a timeout task. */
    public void execute(@Task int task, @TaskType int taskType, Runnable runnable,
            /*@Nullable*/ Runnable timeOutRunnable, long timeoutMillis) {
        countTask(taskType);
        TaskWrapper taskWrapper = getTaskWrapper(task, taskType, runnable);
        if (timeOutRunnable != null) {
            taskWrapper = new TimeoutTaskWrapper(task, taskType, taskWrapper, timeOutRunnable)
                                  .startTimeout(timeoutMillis);
        }
        // TODO: Log task name instead of task number
        Logger.i(TAG, " - task [%s - d: %s, b: %s]: %s", taskTypeToString(taskType), isDelayed(),
                backlogSize(), task);
        scheduleTask(taskWrapper, taskType);
    }

    private void startStarvationCheck() {
        synchronized (lock) {
            if (starvationCheckTask != null) {
                Logger.i(TAG, "Starvation Checks are already running");
                return;
            }

            if (isDelayed()) {
                Logger.i(TAG, " * Starting starvation checks");
                starvationCheckTask = mainThreadRunner.executeWithDelay(
                        "starvationChecks", new StarvationChecker(), STARVATION_CHECK_MS);
            }
        }
    }

    private void maybeCancelStarvationCheck() {
        synchronized (lock) {
            CancelableTask localTask = starvationCheckTask;
            if (!isDelayed() && localTask != null) {
                Logger.i(TAG, "Cancelling starvation checks");
                localTask.cancel();
                starvationCheckTask = null;
            }
        }
    }

    private final class StarvationChecker implements Runnable {
        @Override
        public void run() {
            // TODO: isDelayed shouldn't be called if starvationCheckTask is properly
            // cancelled.
            if (!isDelayed() && !hasBacklog()) {
                // Quick out, we are not delaying things, this stops starvation checking
                Logger.i(TAG, " * Starvation checks being turned off");
                synchronized (lock) {
                    starvationCheckTask = null;
                }
                return;
            }
            long lastTask = lastTaskFinished.get();
            Logger.i(TAG, " * Starvation Check, last task %s",
                    StringFormattingUtils.formatLogDate(lastTask));
            if (clock.currentTimeMillis() >= lastTask + STARVATION_TIMEOUT_MS) {
                Logger.e(TAG, " - Starvation check failed, stopping the delay and running tasks");
                basicLoggingApi.onInternalError(InternalFeedError.TASK_QUEUE_STARVATION);
                // Reset the delay since things aren't being run
                synchronized (lock) {
                    if (waitingForHeadReset) {
                        waitingForHeadReset = false;
                    }
                    if (!initialized) {
                        initialized = true;
                    }
                    starvationCheckTask = null;
                }
                executeNextTask();
            } else {
                synchronized (lock) {
                    starvationCheckTask = mainThreadRunner.executeWithDelay(
                            "StarvationChecks", this, STARVATION_CHECK_MS);
                }
            }
        }
    }

    private void scheduleTask(TaskWrapper taskWrapper, @TaskType int taskType) {
        if (isDelayed() || hasBacklog()) {
            delayedRunCount++;
            queueTask(taskWrapper, taskType);
        } else {
            immediateRunCount++;
            taskWrapper.runTask();
        }
    }

    private TaskWrapper getTaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
        if (taskType == TaskType.HEAD_RESET) {
            return new HeadResetTaskWrapper(task, taskType, runnable);
        }
        if (taskType == TaskType.HEAD_INVALIDATE) {
            return new HeadInvalidateTaskWrapper(task, taskType, runnable);
        }
        return new TaskWrapper(task, taskType, runnable);
    }

    private void queueTask(TaskWrapper taskWrapper, @TaskType int taskType) {
        synchronized (lock) {
            if (taskType == TaskType.HEAD_INVALIDATE || taskType == TaskType.HEAD_RESET
                    || taskType == TaskType.IMMEDIATE) {
                if (taskType == TaskType.HEAD_INVALIDATE && haveHeadInvalidate()) {
                    Logger.w(TAG, " - Duplicate HeadInvalidate Task Found, ignoring new one");
                    return;
                }
                immediateTasks.add(taskWrapper);
                maxImmediateTasks = Math.max(immediateTasks.size(), maxImmediateTasks);
                // An immediate could be created in a delayed state (invalidate head), so we check
                // to see if we need to run tasks
                if (initialized && (currentTask == null)) {
                    Logger.i(TAG, " - queueTask starting immediate task");
                    executeNextTask();
                }
            } else if (taskType == TaskType.USER_FACING) {
                userTasks.add(taskWrapper);
                maxUserFacingTasks = Math.max(userTasks.size(), maxUserFacingTasks);
            } else {
                backgroundTasks.add(taskWrapper);
                maxBackgroundTasks = Math.max(backgroundTasks.size(), maxBackgroundTasks);
            }
        }
    }

    private boolean haveHeadInvalidate() {
        synchronized (lock) {
            for (TaskWrapper taskWrapper : immediateTasks) {
                if (taskWrapper.taskType == TaskType.HEAD_INVALIDATE) {
                    return true;
                }
            }
        }
        return false;
    }

    private void countTask(@TaskType int taskType) {
        taskCount++;
        if (taskType == TaskType.IMMEDIATE) {
            immediateTaskCount++;
        } else if (taskType == TaskType.HEAD_INVALIDATE) {
            headInvalidateTaskCount++;
        } else if (taskType == TaskType.HEAD_RESET) {
            headResetTaskCount++;
        } else if (taskType == TaskType.USER_FACING) {
            userFacingTaskCount++;
        } else if (taskType == TaskType.BACKGROUND) {
            backgroundTaskCount++;
        }
    }

    /** Indicates that tasks are being delayed until a response is processed */
    public boolean isDelayed() {
        synchronized (lock) {
            return !initialized || waitingForHeadReset;
        }
    }

    /** Returns {@literal true} if no tasks are running and no tasks are enqueued. */
    public boolean isIdle() {
        return !hasBacklog() && currentTask == null;
    }

    /** Returns {@literal true} if there are tests enqueued to run. */
    public boolean hasBacklog() {
        synchronized (lock) {
            return !backgroundTasks.isEmpty() || !userTasks.isEmpty() || !immediateTasks.isEmpty();
        }
    }

    @VisibleForTesting
    boolean hasPendingStarvationCheck() {
        synchronized (lock) {
            return starvationCheckTask != null && !starvationCheckTask.canceled();
        }
    }

    private int backlogSize() {
        synchronized (lock) {
            return backgroundTasks.size() + userTasks.size() + immediateTasks.size();
        }
    }

    private void executeNextTask() {
        lastTaskFinished.set(clock.currentTimeMillis());
        synchronized (lock) {
            TaskWrapper task = null;
            if (!immediateTasks.isEmpty()) {
                task = immediateTasks.remove();
            } else if (!userTasks.isEmpty() && !isDelayed()) {
                task = userTasks.remove();
            } else if (!backgroundTasks.isEmpty() && !isDelayed()) {
                task = backgroundTasks.remove();
            }
            if (task != null) {
                task.runTask();
            }
        }
    }

    /**
     * This class wraps the Runnable as a Runnable. It provides common handling of all tasks. This
     * is the default which supports the basic Task, subclasses will manage state changes.
     */
    private class TaskWrapper implements Runnable {
        @Task
        protected final int task;
        final int taskType;
        protected final Runnable runnable;
        private final long queueTimeMs;

        TaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            this.task = task;
            this.taskType = taskType;
            this.runnable = runnable;
            this.queueTimeMs = clock.elapsedRealtime();
        }

        /** This will run the task on the {@link Executor}. */
        void runTask() {
            executor.execute(this);
        }

        /**
         * Run the task (Runnable) then trigger execution of the next task. Prevent subclasses from
         * overriding this method because it tracks state of tasks on the Executor. Protected
         * methods allow subclasses to customize the {@code run} behavior.
         */
        @Override
        public final void run() {
            long startTimeMs = clock.elapsedRealtime();
            // TODO: Log task name instead of task number
            Logger.i(TAG, "Execute task [%s - d: %s, b: %s]: %s", taskTypeToString(taskType),
                    isDelayed(), backlogSize(), task);
            // maintain the currentTask on the stack encase we queue multiple tasks to the Executor
            TaskWrapper saveTask = currentTask;
            currentTask = this;
            runnable.run();
            postRunnableRun();

            currentTask = saveTask;

            int taskExecutionTime = (int) (clock.elapsedRealtime() - startTimeMs);
            int taskDelayTime = (int) (startTimeMs - queueTimeMs);

            // TODO: Log task name instead of task number
            Logger.i(TAG, " - Finished %s, time %s ms", task, taskExecutionTime);
            basicLoggingApi.onTaskFinished(task, taskDelayTime, taskExecutionTime);
            executeNextTask();
        }

        /** This allows subclasses to run code post execution of the Task itself. */
        protected void postRunnableRun() {}
    }

    /**
     * Initialization will flip the {@link #initialized} state to {@code true} when the
     * initialization task completes.
     */
    private final class InitializationTaskWrapper extends TaskWrapper {
        InitializationTaskWrapper(Runnable runnable) {
            super(Task.TASK_QUEUE_INITIALIZE, TaskType.IMMEDIATE, runnable);
        }

        @Override
        protected void postRunnableRun() {
            synchronized (lock) {
                initialized = true;
            }
        }
    }

    /**
     * HeadReset will run a task which resets $HEAD. It clears the {@link #waitingForHeadReset}
     * state.
     */
    private final class HeadResetTaskWrapper extends TaskWrapper {
        HeadResetTaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            super(task, taskType, runnable);
        }

        @Override
        protected void postRunnableRun() {
            synchronized (lock) {
                waitingForHeadReset = false;
            }
            maybeCancelStarvationCheck();
        }
    }

    /**
     * HeadInvalidate is a task which marks the current head as invalid. The TaskQueue will then be
     * delayed until {@link HeadResetTaskWrapper} has completed. This will set the {@link
     * #waitingForHeadReset}. In addition starvation checks will be started.
     */
    private final class HeadInvalidateTaskWrapper extends TaskWrapper {
        HeadInvalidateTaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            super(task, taskType, runnable);
        }

        @Override
        void runTask() {
            synchronized (lock) {
                waitingForHeadReset = true;
            }
            super.runTask();
        }

        @Override
        protected void postRunnableRun() {
            startStarvationCheck();
        }
    }

    /**
     * Runs a Task which has a timeout. The timeout task (Runnable) will run if the primary task is
     * not started before the timeout millis.
     */
    private final class TimeoutTaskWrapper extends TaskWrapper {
        private final AtomicBoolean started = new AtomicBoolean(false);
        private final Runnable timeoutRunnable;
        /*@Nullable*/ private CancelableTask timeoutTask;

        TimeoutTaskWrapper(@Task int task, @TaskType int taskType, Runnable taskRunnable,
                Runnable timeoutRunnable) {
            super(task, taskType, taskRunnable);
            this.timeoutRunnable = timeoutRunnable;
        }

        /**
         * Start the timeout period. If we reach the timeout before the Task is run, the {@link
         * #timeoutRunnable} will run.
         */
        private TimeoutTaskWrapper startTimeout(long timeoutMillis) {
            timeoutTask = mainThreadRunner.executeWithDelay(
                    "taskTimeout", this::runTimeoutCallback, timeoutMillis);
            return this;
        }

        @Override
        void runTask() {
            // If the boolean is already set then runTimeoutCallback has run.
            if (started.getAndSet(true)) {
                Logger.w(TAG, " - runTimeoutCallback already ran [%s]", task);
                executeNextTask();
                return;
            }

            CancelableTask localTask = timeoutTask;
            if (localTask != null) {
                Logger.i(TAG, "Cancelling timeout [%s]", task);
                localTask.cancel();
                timeoutTask = null;
            }

            super.runTask();
        }

        private void runTimeoutCallback() {
            // If the boolean is already set then runTask has run.
            if (started.getAndSet(true)) {
                Logger.w(TAG, " - runTask already ran [%s]", task);
                return;
            }

            Logger.w(TAG, "Execute Timeout [%s]: %s", taskType, task);
            executor.execute(timeoutRunnable);
        }
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("tasks").value(taskCount);
        dumper.forKey("immediateRun").value(immediateRunCount).compactPrevious();
        dumper.forKey("delayedRun").value(delayedRunCount).compactPrevious();

        dumper.forKey("immediateTasks").value(immediateTaskCount);
        dumper.forKey("headInvalidateTasks").value(headInvalidateTaskCount).compactPrevious();
        dumper.forKey("headResetTasks").value(headResetTaskCount).compactPrevious();
        dumper.forKey("userFacingTasks").value(userFacingTaskCount).compactPrevious();
        dumper.forKey("backgroundTasks").value(backgroundTaskCount).compactPrevious();

        dumper.forKey("maxImmediateQueue").value(maxImmediateTasks);
        dumper.forKey("maxUserFacingQueue").value(maxUserFacingTasks).compactPrevious();
        dumper.forKey("maxBackgroundQueue").value(maxBackgroundTasks).compactPrevious();
    }

    private static String taskTypeToString(@TaskType int taskType) {
        switch (taskType) {
            case TaskType.IMMEDIATE:
                return "IMMEDIATE";
            case TaskType.HEAD_INVALIDATE:
                return "HEAD_INVALIDATE";
            case TaskType.HEAD_RESET:
                return "HEAD_RESET";
            case TaskType.USER_FACING:
                return "USER_FACING";
            case TaskType.BACKGROUND:
                return "BACKGROUND";
            default:
                return "UNKNOWN";
        }
    }
}
