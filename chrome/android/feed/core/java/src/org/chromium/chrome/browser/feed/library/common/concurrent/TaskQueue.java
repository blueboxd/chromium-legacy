// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.concurrent;

import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.logging.StringFormattingUtils;
import org.chromium.chrome.browser.feed.library.common.time.Clock;

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

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Queue<TaskWrapper> mImmediateTasks = new ArrayDeque<>();

    @GuardedBy("mLock")
    private final Queue<TaskWrapper> mUserTasks = new ArrayDeque<>();

    @GuardedBy("mLock")
    private final Queue<TaskWrapper> mBackgroundTasks = new ArrayDeque<>();

    @GuardedBy("mLock")
    private boolean mWaitingForHeadReset;

    @GuardedBy("mLock")
    private boolean mInitialized;

    /**
     * CancelableTask that tracks the current starvation runnable. {@liternal null} means that
     * starvation checks are not running.
     */
    @GuardedBy("mLock")
    /*@Nullable*/
    private CancelableTask mStarvationCheckTask;

    // Tracks the current task running on the executor
    /*@Nullable*/ private TaskWrapper mCurrentTask;

    /** Track the time the last task finished. Used for Starvation checks. */
    private final AtomicLong mLastTaskFinished = new AtomicLong();

    private final BasicLoggingApi mBasicLoggingApi;
    private final Executor mExecutor;
    private final Clock mClock;
    private final MainThreadRunner mMainThreadRunner;

    // counters used for dump
    protected int mTaskCount;
    protected int mImmediateRunCount;
    protected int mDelayedRunCount;
    protected int mImmediateTaskCount;
    protected int mHeadInvalidateTaskCount;
    protected int mHeadResetTaskCount;
    protected int mUserFacingTaskCount;
    protected int mBackgroundTaskCount;
    protected int mMaxImmediateTasks;
    protected int mMaxUserFacingTasks;
    protected int mMaxBackgroundTasks;

    public TaskQueue(BasicLoggingApi basicLoggingApi, Executor executor,
            MainThreadRunner mainThreadRunner, Clock clock) {
        this.mBasicLoggingApi = basicLoggingApi;
        this.mExecutor = executor;
        this.mMainThreadRunner = mainThreadRunner;
        this.mClock = clock;
    }

    /** Returns {@code true} if we are delaying for a request */
    public boolean isMakingRequest() {
        synchronized (mLock) {
            return mWaitingForHeadReset;
        }
    }

    /**
     * This method will reset the task queue. This means we will delay all tasks created until the
     * {@link #initialize(Runnable)} task is called again. Also any currently delayed tasks will be
     * removed from the Queue and not run.
     */
    public void reset() {
        synchronized (mLock) {
            mWaitingForHeadReset = false;
            mInitialized = false;

            // clear all delayed tasks
            Logger.i(TAG, " - Reset i: %s, u: %s, b: %s", mImmediateTasks.size(), mUserTasks.size(),
                    mBackgroundTasks.size());
            mImmediateTasks.clear();
            mUserTasks.clear();
            mBackgroundTasks.clear();

            // Since we are delaying thing, start the starvation checker
            startStarvationCheck();
        }
    }

    /** this is called post reset to clear the initialized flag. */
    public void completeReset() {
        Logger.i(TAG, "completeReset");
        synchronized (mLock) {
            mInitialized = true;
        }

        maybeCancelStarvationCheck();
    }

    /**
     * Called to initialize the {@link FeedSessionManager}. This needs to be the first task run, all
     * other tasks are delayed until initialization finishes.
     */
    public void initialize(Runnable runnable) {
        synchronized (mLock) {
            if (mInitialized) {
                Logger.w(TAG, " - Calling initialize on an initialized TaskQueue");
            }
        }

        TaskWrapper task = new InitializationTaskWrapper(runnable);
        countTask(task.mTaskType);
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
        synchronized (mLock) {
            if (mStarvationCheckTask != null) {
                Logger.i(TAG, "Starvation Checks are already running");
                return;
            }

            if (isDelayed()) {
                Logger.i(TAG, " * Starting starvation checks");
                mStarvationCheckTask = mMainThreadRunner.executeWithDelay(
                        "starvationChecks", new StarvationChecker(), STARVATION_CHECK_MS);
            }
        }
    }

    private void maybeCancelStarvationCheck() {
        synchronized (mLock) {
            CancelableTask localTask = mStarvationCheckTask;
            if (!isDelayed() && localTask != null) {
                Logger.i(TAG, "Cancelling starvation checks");
                localTask.cancel();
                mStarvationCheckTask = null;
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
                synchronized (mLock) {
                    mStarvationCheckTask = null;
                }
                return;
            }
            long lastTask = mLastTaskFinished.get();
            Logger.i(TAG, " * Starvation Check, last task %s",
                    StringFormattingUtils.formatLogDate(lastTask));
            if (mClock.currentTimeMillis() >= lastTask + STARVATION_TIMEOUT_MS) {
                Logger.e(TAG, " - Starvation check failed, stopping the delay and running tasks");
                mBasicLoggingApi.onInternalError(InternalFeedError.TASK_QUEUE_STARVATION);
                // Reset the delay since things aren't being run
                synchronized (mLock) {
                    if (mWaitingForHeadReset) {
                        mWaitingForHeadReset = false;
                    }
                    if (!mInitialized) {
                        mInitialized = true;
                    }
                    mStarvationCheckTask = null;
                }
                executeNextTask();
            } else {
                synchronized (mLock) {
                    mStarvationCheckTask = mMainThreadRunner.executeWithDelay(
                            "StarvationChecks", this, STARVATION_CHECK_MS);
                }
            }
        }
    }

    private void scheduleTask(TaskWrapper taskWrapper, @TaskType int taskType) {
        if (isDelayed() || hasBacklog()) {
            mDelayedRunCount++;
            queueTask(taskWrapper, taskType);
        } else {
            mImmediateRunCount++;
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
        synchronized (mLock) {
            if (taskType == TaskType.HEAD_INVALIDATE || taskType == TaskType.HEAD_RESET
                    || taskType == TaskType.IMMEDIATE) {
                if (taskType == TaskType.HEAD_INVALIDATE && haveHeadInvalidate()) {
                    Logger.w(TAG, " - Duplicate HeadInvalidate Task Found, ignoring new one");
                    return;
                }
                mImmediateTasks.add(taskWrapper);
                mMaxImmediateTasks = Math.max(mImmediateTasks.size(), mMaxImmediateTasks);
                // An immediate could be created in a delayed state (invalidate head), so we check
                // to see if we need to run tasks
                if (mInitialized && (mCurrentTask == null)) {
                    Logger.i(TAG, " - queueTask starting immediate task");
                    executeNextTask();
                }
            } else if (taskType == TaskType.USER_FACING) {
                mUserTasks.add(taskWrapper);
                mMaxUserFacingTasks = Math.max(mUserTasks.size(), mMaxUserFacingTasks);
            } else {
                mBackgroundTasks.add(taskWrapper);
                mMaxBackgroundTasks = Math.max(mBackgroundTasks.size(), mMaxBackgroundTasks);
            }
        }
    }

    private boolean haveHeadInvalidate() {
        synchronized (mLock) {
            for (TaskWrapper taskWrapper : mImmediateTasks) {
                if (taskWrapper.mTaskType == TaskType.HEAD_INVALIDATE) {
                    return true;
                }
            }
        }
        return false;
    }

    private void countTask(@TaskType int taskType) {
        mTaskCount++;
        if (taskType == TaskType.IMMEDIATE) {
            mImmediateTaskCount++;
        } else if (taskType == TaskType.HEAD_INVALIDATE) {
            mHeadInvalidateTaskCount++;
        } else if (taskType == TaskType.HEAD_RESET) {
            mHeadResetTaskCount++;
        } else if (taskType == TaskType.USER_FACING) {
            mUserFacingTaskCount++;
        } else if (taskType == TaskType.BACKGROUND) {
            mBackgroundTaskCount++;
        }
    }

    /** Indicates that tasks are being delayed until a response is processed */
    public boolean isDelayed() {
        synchronized (mLock) {
            return !mInitialized || mWaitingForHeadReset;
        }
    }

    /** Returns {@literal true} if no tasks are running and no tasks are enqueued. */
    public boolean isIdle() {
        return !hasBacklog() && mCurrentTask == null;
    }

    /** Returns {@literal true} if there are tests enqueued to run. */
    public boolean hasBacklog() {
        synchronized (mLock) {
            return !mBackgroundTasks.isEmpty() || !mUserTasks.isEmpty()
                    || !mImmediateTasks.isEmpty();
        }
    }

    @VisibleForTesting
    boolean hasPendingStarvationCheck() {
        synchronized (mLock) {
            return mStarvationCheckTask != null && !mStarvationCheckTask.canceled();
        }
    }

    private int backlogSize() {
        synchronized (mLock) {
            return mBackgroundTasks.size() + mUserTasks.size() + mImmediateTasks.size();
        }
    }

    private void executeNextTask() {
        mLastTaskFinished.set(mClock.currentTimeMillis());
        synchronized (mLock) {
            TaskWrapper task = null;
            if (!mImmediateTasks.isEmpty()) {
                task = mImmediateTasks.remove();
            } else if (!mUserTasks.isEmpty() && !isDelayed()) {
                task = mUserTasks.remove();
            } else if (!mBackgroundTasks.isEmpty() && !isDelayed()) {
                task = mBackgroundTasks.remove();
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
        protected final int mTask;
        final int mTaskType;
        protected final Runnable mRunnable;
        private final long mQueueTimeMs;

        TaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            this.mTask = task;
            this.mTaskType = taskType;
            this.mRunnable = runnable;
            this.mQueueTimeMs = mClock.elapsedRealtime();
        }

        /** This will run the task on the {@link Executor}. */
        void runTask() {
            mExecutor.execute(this);
        }

        /**
         * Run the task (Runnable) then trigger execution of the next task. Prevent subclasses from
         * overriding this method because it tracks state of tasks on the Executor. Protected
         * methods allow subclasses to customize the {@code run} behavior.
         */
        @Override
        public final void run() {
            long startTimeMs = mClock.elapsedRealtime();
            // TODO: Log task name instead of task number
            Logger.i(TAG, "Execute task [%s - d: %s, b: %s]: %s", taskTypeToString(mTaskType),
                    isDelayed(), backlogSize(), mTask);
            // maintain the currentTask on the stack encase we queue multiple tasks to the Executor
            TaskWrapper saveTask = mCurrentTask;
            mCurrentTask = this;
            mRunnable.run();
            postRunnableRun();

            mCurrentTask = saveTask;

            int taskExecutionTime = (int) (mClock.elapsedRealtime() - startTimeMs);
            int taskDelayTime = (int) (startTimeMs - mQueueTimeMs);

            // TODO: Log task name instead of task number
            Logger.i(TAG, " - Finished %s, time %s ms", mTask, taskExecutionTime);
            mBasicLoggingApi.onTaskFinished(mTask, taskDelayTime, taskExecutionTime);
            executeNextTask();
        }

        /** This allows subclasses to run code post execution of the Task itself. */
        protected void postRunnableRun() {}
    }

    /**
     * Initialization will flip the {@link #mInitialized} state to {@code true} when the
     * initialization task completes.
     */
    private final class InitializationTaskWrapper extends TaskWrapper {
        InitializationTaskWrapper(Runnable runnable) {
            super(Task.TASK_QUEUE_INITIALIZE, TaskType.IMMEDIATE, runnable);
        }

        @Override
        protected void postRunnableRun() {
            synchronized (mLock) {
                mInitialized = true;
            }
        }
    }

    /**
     * HeadReset will run a task which resets $HEAD. It clears the {@link #mWaitingForHeadReset}
     * state.
     */
    private final class HeadResetTaskWrapper extends TaskWrapper {
        HeadResetTaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            super(task, taskType, runnable);
        }

        @Override
        protected void postRunnableRun() {
            synchronized (mLock) {
                mWaitingForHeadReset = false;
            }
            maybeCancelStarvationCheck();
        }
    }

    /**
     * HeadInvalidate is a task which marks the current head as invalid. The TaskQueue will then be
     * delayed until {@link HeadResetTaskWrapper} has completed. This will set the {@link
     * #mWaitingForHeadReset}. In addition starvation checks will be started.
     */
    private final class HeadInvalidateTaskWrapper extends TaskWrapper {
        HeadInvalidateTaskWrapper(@Task int task, @TaskType int taskType, Runnable runnable) {
            super(task, taskType, runnable);
        }

        @Override
        void runTask() {
            synchronized (mLock) {
                mWaitingForHeadReset = true;
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
        private final AtomicBoolean mStarted = new AtomicBoolean(false);
        private final Runnable mTimeoutRunnable;
        /*@Nullable*/ private CancelableTask mTimeoutTask;

        TimeoutTaskWrapper(@Task int task, @TaskType int taskType, Runnable taskRunnable,
                Runnable timeoutRunnable) {
            super(task, taskType, taskRunnable);
            this.mTimeoutRunnable = timeoutRunnable;
        }

        /**
         * Start the timeout period. If we reach the timeout before the Task is run, the {@link
         * #mTimeoutRunnable} will run.
         */
        private TimeoutTaskWrapper startTimeout(long timeoutMillis) {
            mTimeoutTask = mMainThreadRunner.executeWithDelay(
                    "taskTimeout", this::runTimeoutCallback, timeoutMillis);
            return this;
        }

        @Override
        void runTask() {
            // If the boolean is already set then runTimeoutCallback has run.
            if (mStarted.getAndSet(true)) {
                Logger.w(TAG, " - runTimeoutCallback already ran [%s]", mTask);
                executeNextTask();
                return;
            }

            CancelableTask localTask = mTimeoutTask;
            if (localTask != null) {
                Logger.i(TAG, "Cancelling timeout [%s]", mTask);
                localTask.cancel();
                mTimeoutTask = null;
            }

            super.runTask();
        }

        private void runTimeoutCallback() {
            // If the boolean is already set then runTask has run.
            if (mStarted.getAndSet(true)) {
                Logger.w(TAG, " - runTask already ran [%s]", mTask);
                return;
            }

            Logger.w(TAG, "Execute Timeout [%s]: %s", mTaskType, mTask);
            mExecutor.execute(mTimeoutRunnable);
        }
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("tasks").value(mTaskCount);
        dumper.forKey("immediateRun").value(mImmediateRunCount).compactPrevious();
        dumper.forKey("delayedRun").value(mDelayedRunCount).compactPrevious();

        dumper.forKey("immediateTasks").value(mImmediateTaskCount);
        dumper.forKey("headInvalidateTasks").value(mHeadInvalidateTaskCount).compactPrevious();
        dumper.forKey("headResetTasks").value(mHeadResetTaskCount).compactPrevious();
        dumper.forKey("userFacingTasks").value(mUserFacingTaskCount).compactPrevious();
        dumper.forKey("backgroundTasks").value(mBackgroundTaskCount).compactPrevious();

        dumper.forKey("maxImmediateQueue").value(mMaxImmediateTasks);
        dumper.forKey("maxUserFacingQueue").value(mMaxUserFacingTasks).compactPrevious();
        dumper.forKey("maxBackgroundQueue").value(mMaxBackgroundTasks).compactPrevious();
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
