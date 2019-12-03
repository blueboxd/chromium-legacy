// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.time;

import android.text.TextUtils;
import android.util.LongSparseArray;

import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Queue;
import java.util.Stack;

import javax.annotation.concurrent.GuardedBy;

/**
 * Utility class providing timing related utilities. The primary feature is to created {@link
 * ElapsedTimeTracker} instances, which are used to track elapsed time for tasks. The timing
 * information is output to the state dump.
 */
public class TimingUtils implements Dumpable {
    private static final String TAG = "TimingUtils";
    private static final String BACKGROUND_THREAD = "background-";
    private static final String UI_THREAD = "ui";
    private static final int MAX_TO_DUMP = 10;

    private static int sBgThreadId = 1;

    private final ThreadUtils mThreadUtils = new ThreadUtils();
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Queue<ThreadState> mThreadDumps = new ArrayDeque<>(MAX_TO_DUMP);

    @GuardedBy("mLock")
    private final LongSparseArray<ThreadStack> mThreadStacks = new LongSparseArray<>();

    /**
     * ElapsedTimeTracker works similar to Stopwatch. This is used to track elapsed time for some
     * task. The start time is tracked when the instance is created. {@code stop} is used to capture
     * the end time and other statistics about the task. The ElapsedTimeTrackers dumped through the
     * Dumper. A stack is maintained to log sub-tasks with proper indentation in the Dumper.
     *
     * <p>The class will dump only the {@code MAX_TO_DUMP} most recent dumps, discarding older dumps
     * when new dumps are created.
     *
     * <p>ElapsedTimeTracker is designed as a one use class, {@code IllegalStateException}s are
     * thrown if the class isn't used correctly.
     */
    public static class ElapsedTimeTracker {
        private final ThreadStack mThreadStack;
        private final String mSource;

        private final long mStartTime;
        private long mEndTime;

        private ElapsedTimeTracker(ThreadStack threadStack, String source) {
            this.mThreadStack = threadStack;
            this.mSource = source;
            mStartTime = System.nanoTime();
        }

        /**
         * Capture the end time for the elapsed time. {@code IllegalStateException} is thrown if
         * stop is called more than once. Arguments are treated as pairs within the Dumper output.
         *
         * <p>For example: dumper.forKey(arg[0]).value(arg[1])
         */
        public void stop(Object... args) {
            if (mEndTime > 0) {
                throw new IllegalStateException("ElapsedTimeTracker has already been stopped.");
            }
            mEndTime = System.nanoTime();
            TrackerState trackerState = new TrackerState(
                    mEndTime - mStartTime, mSource, args, mThreadStack.mStack.size());
            mThreadStack.addTrackerState(trackerState);
            mThreadStack.popElapsedTimeTracker(this);
        }
    }

    /**
     * Return a new {@link ElapsedTimeTracker} which is added to the Thread scoped stack. When we
     * dump the tracker, we will indent the source to indicate sub-tasks within a larger task.
     */
    public ElapsedTimeTracker getElapsedTimeTracker(String source) {
        long threadId = Thread.currentThread().getId();
        synchronized (mLock) {
            ThreadStack timerStack = mThreadStacks.get(threadId);
            if (timerStack == null) {
                timerStack = new ThreadStack(
                        mThreadUtils.isMainThread() ? UI_THREAD : BACKGROUND_THREAD + sBgThreadId++,
                        false);
                mThreadStacks.put(threadId, timerStack);
            }
            ElapsedTimeTracker timeTracker = new ElapsedTimeTracker(timerStack, source);
            timerStack.mStack.push(timeTracker);
            return timeTracker;
        }
    }

    /**
     * This is called to pin the stack structure for a thread. This should only be done for threads
     * which are long lived. Non-pinned thread will have their stack structures clean up when the
     * stack is empty.
     */
    public void pinThread(Thread thread, String name) {
        ThreadStack timerStack = new ThreadStack(name, true);
        synchronized (mLock) {
            mThreadStacks.put(thread.getId(), timerStack);
        }
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        synchronized (mLock) {
            for (ThreadState threadState : mThreadDumps) {
                dumpThreadState(dumper, threadState);
            }
        }
    }

    private void dumpThreadState(Dumper dumper, ThreadState threadState) {
        if (threadState.mTrackerStates.isEmpty()) {
            Logger.w(TAG, "Found Empty TrackerState List");
            return;
        }
        dumper.forKey("thread").value(threadState.mThreadName);
        dumper.forKey("timeStamp").value(threadState.mDate).compactPrevious();
        for (int i = threadState.mTrackerStates.size() - 1; i >= 0; i--) {
            TrackerState trackerState = threadState.mTrackerStates.get(i);
            Dumper child = dumper.getChildDumper();
            child.forKey("time", trackerState.mIndent - 1)
                    .value(trackerState.mDuration / 1000000 + "ms");
            child.forKey("source").value(trackerState.mSource).compactPrevious();
            if (trackerState.mArgs != null && trackerState.mArgs.length > 0) {
                for (int j = 0; j < trackerState.mArgs.length; j++) {
                    String key = trackerState.mArgs[j++].toString();
                    Object value = (j < trackerState.mArgs.length) ? trackerState.mArgs[j] : "";
                    child.forKey(key, trackerState.mIndent - 1)
                            .valueObject(value)
                            .compactPrevious();
                }
            }
        }
    }

    /** Definition of a Stack of {@link ElapsedTimeTracker} instances. */
    private class ThreadStack {
        final String mName;
        final Stack<ElapsedTimeTracker> mStack = new Stack<>();
        private List<TrackerState> mTrackerStates = new ArrayList<>();
        final boolean mPin;

        ThreadStack(String name, boolean pin) {
            this.mName = name;
            this.mPin = pin;
        }

        void addTrackerState(TrackerState trackerState) {
            mTrackerStates.add(trackerState);
        }

        void popElapsedTimeTracker(ElapsedTimeTracker tracker) {
            ElapsedTimeTracker top = mStack.peek();
            if (top != tracker) {
                int pos = mStack.search(tracker);
                if (pos == -1) {
                    Logger.w(TAG, "Trying to Pop non-top of stack timer, ignoring");
                    return;
                } else {
                    int c = 0;
                    while (mStack.peek() != tracker) {
                        c++;
                        mStack.pop();
                    }
                    Logger.w(TAG, "Pop TimingTracker which was not the current top, popped % items",
                            c);
                }
            }
            mStack.pop();
            if (mStack.isEmpty()) {
                StringBuilder sb = new StringBuilder();
                TrackerState ts = mTrackerStates.get(mTrackerStates.size() - 1);
                for (int i = 0; i < ts.mArgs.length; i++) {
                    String key = ts.mArgs[i++].toString();
                    Object value = (i < ts.mArgs.length) ? ts.mArgs[i] : "";
                    if (!TextUtils.isEmpty(key)) {
                        sb.append(key).append(" : ").append(value);
                    } else {
                        sb.append(value);
                    }
                    if ((i + 1) < ts.mArgs.length) {
                        sb.append(" | ");
                    }
                }
                Logger.i(TAG, "Task Timing %3sms, thread %s | %s",
                        ((tracker.mEndTime - tracker.mStartTime) / 1000000),
                        tracker.mThreadStack.mName, sb);
                synchronized (mLock) {
                    if (mThreadDumps.size() == MAX_TO_DUMP) {
                        // Before adding a new tracker state, remove the oldest one.
                        mThreadDumps.remove();
                    }
                    mThreadDumps.add(new ThreadState(mTrackerStates, mName));
                    mTrackerStates = new ArrayList<>();
                    if (!mPin) {
                        mThreadStacks.remove(Thread.currentThread().getId());
                    }
                }
            }
        }
    }

    /** State associated with a thread */
    private static class ThreadState {
        final List<TrackerState> mTrackerStates;
        final String mThreadName;
        final Date mDate;

        ThreadState(List<TrackerState> trackerStates, String threadName) {
            this.mTrackerStates = trackerStates;
            this.mThreadName = threadName;
            mDate = new Date();
        }
    }

    /** State associated with a completed ElapsedTimeTracker */
    private static class TrackerState {
        final long mDuration;
        final String mSource;
        final Object[] mArgs;
        final int mIndent;

        TrackerState(long duration, String source, Object[] args, int indent) {
            this.mDuration = duration;
            this.mSource = source;
            this.mArgs = args;
            this.mIndent = indent;
        }
    }
}
